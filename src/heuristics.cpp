#include "buffer.hpp"
#include "heuristics.hpp"

#include<unistd.h>


/** compute entropy based on terminals of the given dd_edge **/ 
double dd_heuristic::compute_entropy(MEDDLY::dd_edge& dd) {
    std::set<int> occurrences; 
    double acc = 0, h = 0;
    
    //collect terminals
    for (MEDDLY::enumerator it(dd); it; ++it) {
        int x; 
        it.getValue(x); 
        occurrences.insert(x); 
        acc += x; 
    }

    //compute entropy applying the p*log(p) formula
    for (const int& n_occ: occurrences) {
        double p = n_occ / static_cast<double>(acc);
        h += p * log2(p); 
    } 

    return -h;
}

/** build a mtmdd counting substrings given by variable in the partial order and 
 * the current variable  */
double dd_heuristic::entropy_metric(const int var) {  
    //vars are those of partial ordering, the current variable var and the terminal values 
    int curr_numvars = partial_ordering.size() + 2; 
    //the extra element in the buffer is required by Meddly 
    Buffer& buffer = *this->buffer; 
    std::vector<long> terminals(buffer.size(), 1); 
    std::vector<int> current_ordering(partial_ordering), 
                     *slot = nullptr; 

    current_ordering.push_back(var); 
    MEDDLY::dd_edge& edge(edges.back()); 

    // std::cout << "Current ordering: \n";
    // for (auto x: current_ordering)std::cout << x << " ";std::cout << "\n"; 

    for (MEDDLY::enumerator e(*dd.root); e; ++e) {
        const int *p = e.getAssignments(); 
        int curr_index = curr_numvars - 1;
        bool no_time2flush = buffer.get_slot(slot);
        
        for (const int& x: current_ordering) 
            slot->at(curr_index--) = p[x]; 
        //save current terminal value 
        e.getValue(slot->back());
        slot->front() = -1;
        
        //if the buffer is filled, flush data into mtmdd 
        if (no_time2flush == false) {
            buffer.flush(edge, &terminals); 
        }
    }

    //flush data from buffer to dd
    if (buffer.num_elements() > 0) {
        buffer.flush(edge, &terminals); 
    }

    return compute_entropy(edge);
}


void dd_heuristic::entropy_heuristic() {
    do {

        mtmdd::domain_bounds_t bounds(partial_ordering.size() + 2, -30);  //extensible domain -- should be 'fixed' using encoders   
        bounds.front() = 0; 
        
        MEDDLY::forest::policies policy = MEDDLY::forest::policies(false); 
        policy.setSparseStorage();
        domain = MEDDLY::createDomainBottomUp(bounds.data(), bounds.size());
        forest = domain->createForest(false, MEDDLY::forest::INTEGER, MEDDLY::forest::MULTI_TERMINAL, policy); 
        buffer = new Buffer(100000, partial_ordering.size() + 3, false); 

        // std::cout << "building forest using " << bounds.size() << " variables" << std::endl;

        int best_index = -1; 
        order_metric best(-1, 1000); //

        for (int i = 0; i < unbounded_vars.size(); ++i) {
            int curr_val = unbounded_vars.at(i); 
            edges.emplace_back(forest); //create new dd edge 
            //calculate dd counting k-mers based on partial ordering and current variable
            order_metric current(curr_val, entropy_metric(curr_val));

            if (best > current) {
                best = current; 
                best_index = i; 
            }
        }

        partial_ordering.push_back(unbounded_vars.at(best_index)); 
        unbounded_vars.erase(unbounded_vars.begin() + best_index); 

        edges.clear();
        MEDDLY::destroyForest(forest);
        MEDDLY::destroyDomain(domain); 
        delete buffer; 
        
    } while (unbounded_vars.size() > 1); 

    partial_ordering.push_back(unbounded_vars.at(0));
    unbounded_vars.clear();
}







int main(int argc, char** argv) {
    MEDDLY::initialize(MEDDLY::defaultInitializerList(NULL)); 

    cxxopts::Options options(argv[0], "GRAPES-DD"); 
    options.add_options()
        ("i, in", "graph database filename", cxxopts::value<std::string>())
        ("l, lp", "max pathlength", cxxopts::value<int>()->default_value("4"))
        ("d, direct", "are graph direct?", cxxopts::value<std::string>()->default_value("true"));

    cxxopts::ParseResult result = options.parse(argc, argv); 
    std::string graph_file, output_folder; 
    int max_depth;
    bool direct_graph; 


    try {
        graph_file.assign(result["in"].as<std::string>()); 
        output_folder.assign(graph_file.substr(0, graph_file.find_last_of("/"))); 
        max_depth = result["lp"].as<int>();
        direct_graph = result["direct"].as<std::string>().compare("true") == 0; 
    } catch (std::domain_error& ex) {
        std::cout << "Missing IO parameters!\n";
        std::cout << options.help() << std::endl; 
        return 1; 
    }

    mtmdd::MultiterminalDecisionDiagram index_dd; 

    if (grapes2dd::dd_already_indexed(graph_file, max_depth)) {
        std::cout << "Loading DD from index file" << std::endl; 
        index_dd.read(graph_file, max_depth);
    } else {
        std::cout << "Indexing graphs" << std::endl; 
        GraphsDB db(graph_file, direct_graph);
        index_dd.init(db, max_depth); 
        index_dd.write(graph_file);
    }

    mtmdd::var_order_t ordering; 
    index_dd.get_variable_ordering(ordering); 

    dd_heuristic x(index_dd);
    x.entropy_heuristic();

}