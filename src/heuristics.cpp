#include "buffer.hpp"
#include "heuristics.hpp"
#include <algorithm>
#include <tuple>


/** Initialize a MEDDLY forest for MTMDDs with N variables, 
 * where N is the number of variables already in the partial ordering. 
 * If use_terminals is set to true, the forest will have N+1 variables. 
 * The number of variables is returned.  */ 
int heuristic::init_forest(bool use_terminals) { 
    //considering variables in the order, the current variable and, optionally, terminal values.
    int num_dd_vars = partial_ordering.size() + 1 + (int) use_terminals;
    mtmdd::domain_bounds_t bounds(num_dd_vars, -30); //need to define variable bounds
    MEDDLY::forest::policies policy = MEDDLY::forest::policies(false); 
    policy.setSparseStorage(); 
    domain = MEDDLY::createDomainBottomUp(bounds.data(), bounds.size());
    forest = domain->createForest(false, MEDDLY::forest::INTEGER, MEDDLY::forest::MULTI_TERMINAL, policy);
    buffer = new Buffer(bufferlength, num_dd_vars + 1, true); 

    // std::cout << "partial ordering size: " << partial_ordering.size() << std::endl;
    // std::cout << "init forest with " << num_dd_vars << " variables!" << std::endl; 

    return num_dd_vars; 
}

/** Destroy the current MEDDLY forest */ 
void heuristic::clear_forest() {
    edges.clear(); 
    MEDDLY::destroyForest(forest); 
    MEDDLY::destroyDomain(domain); 
    delete buffer; 
}


/** Build a MTMDD from the index MTMDD by considering only those variables in the partial ordering, 
 * plus the current var. Terminal values can be calculated summing either 
 * (i) one for each feature or (ii) the terminal values of the index MTMDD.
 * 
 * The edge representing the current MTMDD is returned. */
MEDDLY::dd_edge& heuristic::build_mtmdd(int var, bool terminal_as_counts) {
    //vars are those of partial ordering, the current variable var and the (optional) terminal values
    // int curr_numvars = partial_ordering.size() + 1 + (int) use_terminals; 
    std::vector<int> current_ordering(partial_ordering); 
    current_ordering.push_back(var); 

    // std::cout << "Current ordering: \n";
    // for (auto x: current_ordering)std::cout << x << " ";std::cout << "\n"; 

    buffer->flush();            //empty buffer 
    edges.emplace_back(forest); //create dd edge for current ordering 
    load_dd_data(current_ordering); //load data into dd edge 
    
    //flush remaining data into dd edge 
    std::vector<long>* t = terminal_as_counts 
        ? nullptr       //if you use terminal as counts, proper values will be in the buffer
        : &terminals;   //otherwise, if you simply count k-mer occurrences, explicitly pass a vector filled with ones. 
    buffer->flush(edges.back(), t); 
    return edges.back(); 
}


/** Transfer data from the index MTMDD to the current edge by considering 
 * only the variables in the order passed as parameter  */
void heuristic::load_dd_data(const mtmdd::var_order_t& order) {
    Buffer& buffer = *this->buffer;
    std::vector<int> *slot = nullptr;
    MEDDLY::dd_edge& edge(edges.back()); 

    for (MEDDLY::enumerator e(*index.root); e; ++e) {
        bool no_time2flush = buffer.get_slot(slot); 
        const int *p = e.getAssignments();
        int curr_index = order.size(); 

        e.getValue( slot->back() ); //this value is overwritten if terminal values are not being used as variable 
        buffer.save_value( slot->back() ); 
        slot->front() = -1; 
        //copy values of variables belonging to the current ordering 
        for (const int& x: order)       
            slot->at(curr_index--) = p[x];          
        
        if (no_time2flush == false) 
            buffer.flush(edge, &terminals); 
    }
}

/** 
 * */
const mtmdd::var_order_t& heuristic::get() {
    do {
        int num_dd_vars = init_forest(false); //to parametrize 
        order_metric best, current; 
        mtmdd::var_order_t::iterator best_var;

        for (mtmdd::var_order_t::iterator it = unbounded_vars.begin(); it != unbounded_vars.end(); ++it) {
            order_metric current(get_metric(*it)); 

            if (!best || current.is_better_than(best)) {
                best_var = it; 
                best = current; 
            }
        }

        partial_ordering.push_back(*best_var); 
        unbounded_vars.erase(best_var); 
        clear_forest(); 

    } while (unbounded_vars.size() > 1);

    partial_ordering.push_back(unbounded_vars.front());
    return partial_ordering;
}

//entropy metric
order_metric entropy_heuristic::compute_metric(const MEDDLY::dd_edge& edge)  {
    std::vector<int> occurrences; 
    double h, sum_elems = 0; 
    long num_elems; 

    MEDDLY::apply(MEDDLY::CARDINALITY, edge, num_elems); 
    occurrences.reserve(num_elems);

    for (MEDDLY::enumerator e(edge); e; ++e) {
        int tmp; 
        e.getValue(tmp); 
        occurrences.push_back(tmp); 
        sum_elems += tmp; 
    }

    h = std::accumulate(occurrences.begin(), occurrences.end(), 0., [sum_elems](double a, int x) {
        double p = x / sum_elems; 
        return a + p * log2(p); 
    }); 

    return order_metric(current_var, h); //h < 0 so that you can maximise instead of minimize
}

//correlation metric 
order_metric correlation_heuristic::compute_metric(const MEDDLY::dd_edge& edge) {
    using my_triple = std::tuple<unsigned, unsigned, long>; 

    int num_vars = edge.getForest()->getDomain()->getNumVariables();
    encoder<std::string> enc_v1;
    encoder<int> enc_v2; 

    // std::cout << "num variables: " << num_vars << std::endl; 

    std::vector<my_triple> sparse_matrix; 
    long dim; 
    MEDDLY::apply(MEDDLY::CARDINALITY, edge, dim); 
    sparse_matrix.reserve(dim); 

    for (MEDDLY::enumerator e(edge); e; ++e) {
        const int *p = e.getAssignments(); 
        std::stringstream ss;
        int term = 0; 

        for (int i = 1; i < num_vars; ++i)  ss << p[i] << ","; //concatenate multiple variables 

        // std::cout << "encoded: " << ss.str() << "\n";

        e.getValue(term); 
        sparse_matrix.emplace_back(
            enc_v1.get(ss.str()), 
            enc_v2.get(p[num_vars]), 
            term);
    }

    //init empty |v1| x |v2| matrix 
    contingency_table contingency(enc_v1.size(), enc_v2.size()); 

    for (const my_triple& t: sparse_matrix) {
        unsigned i, j; long count; 
        std::tie(i, j, count) = t;
        contingency.add(i, j, count);
    }

    // contingency.show();

    return order_metric(current_var, contingency.cramerV());        
}


void correlation_heuristic::init_ordering() {
    // using my_pair = std::pair<int, order_metric>;
    using it_pair = std::pair<mtmdd::var_order_t::iterator, mtmdd::var_order_t::iterator>; 

    it_pair best_vars; 
    double h = 0; 

    //test all possible pair of variables in order to find the most correlated pair  
    partial_ordering.push_back(0);
    init_forest(false); //create a forest with 2 variables 

    for (mtmdd::var_order_t::iterator it = unbounded_vars.begin(); it != unbounded_vars.end(); ++it) {
        partial_ordering.at(0) = *it; //set first variable 

        for (mtmdd::var_order_t::iterator it2 = it + 1; it2 != unbounded_vars.end(); ++it2) {
            current_var = *it2; //set second variable 
            
            MEDDLY::dd_edge& curr_dd = build_mtmdd(current_var, false); //build dd based on first variable and current one 
            order_metric curr(compute_metric(curr_dd)); 

            // mtmdd::visit_edge(curr_dd); 

            if (curr.get_hval() > h) {
                best_vars.first = it;
                best_vars.second = it2; 
                h = curr.get_hval(); 
            }
        }
    }

    int ivar1 = best_vars.first - unbounded_vars.begin(), ivar2 = best_vars.second - unbounded_vars.begin(); 

    partial_ordering.resize(2);
    partial_ordering[0] = *best_vars.first;
    partial_ordering[1] = *best_vars.second; 

    unbounded_vars.erase(unbounded_vars.begin() + ivar1);
    unbounded_vars.erase(unbounded_vars.begin() + ivar2 - 1); 
    
    clear_forest();
}

