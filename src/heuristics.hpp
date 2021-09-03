#include "mtmdd.hpp"

#include <meddly.h>
#include <meddly_expert.h>
#include <iterator>
#include <set>
#include <vector>
#include <numeric>
#include <algorithm>

#include "mtmdd.hpp"
#include "cxxopts.hpp"

using contingency_row_t = std::vector<uint64_t>;
using contingency_table_t = std::vector< contingency_row_t >;


class order_metric; 
template<class T> class encoder;
class dd_collection;

class heuristic; 
class entropy_heuristic; 
class correlation_heuristic; 
class cache_dd;

class combinations {
public: 
    using combination = std::set<int>; 
private:
    using mapset = std::map<int, std::set<combination>>;

    std::vector<int> sequence; 
    mapset allCombs; 

    void calculate() {
        do {
            for (int i = 0; i < sequence.size(); ++i) {
                combination current(sequence.begin(), sequence.begin() + i + 1);
                mapset::iterator it = allCombs.find(current.size());
                assert(it != allCombs.end());
                it->second.insert(current);                
            }
        } while (std::next_permutation(sequence.begin(), sequence.end()));
    }
public:
    combinations(const std::vector<int> v) : sequence(v) {
        for (int i = 1; i <= sequence.size(); ++i)
            allCombs.emplace(std::piecewise_construct, std::forward_as_tuple(i), std::forward_as_tuple());

        calculate(); 
    }

    inline const std::set<combination>& get(int lseq) {
        return allCombs[lseq]; 
    }

    
}; 




class cache_metric : public std::map< std::set<int>, float> {
public: 

};


class cache_dd {
    mtmdd::StatsDD* p2best;
    std::map<std::string, mtmdd::StatsDD> cache; 

    inline void to_str(const mtmdd::var_order_t& ordering, std::string& res) const {
        std::stringstream ss; 
        ss << ordering[0];
        for (auto it = ordering.begin() + 1; it != ordering.end(); ++it)    
            ss << "," << *it;
        res = ss.str(); 
    }
public:
    cache_dd(): p2best(nullptr) {}

    inline const mtmdd::StatsDD* get(const mtmdd::var_order_t& ordering) const {
        std::string mystr; 
        to_str(ordering, mystr);

        std::map<std::string, mtmdd::StatsDD>::const_iterator it = cache.find(mystr); 
        return it != cache.end() ? &it->second : nullptr; 

    }

    inline const mtmdd::StatsDD* set(const mtmdd::StatsDD& stats) {
        std::string mystr; 
        to_str(stats.ordering, mystr); 

        auto res = cache.emplace(
            std::piecewise_construct, 
            std::forward_as_tuple(mystr), 
            std::forward_as_tuple(stats)
        );
        mtmdd::StatsDD* p = &res.first->second;     
        if (res.second) {
            //update best element 
            if (p2best && res.first->second.memory_used < p2best->memory_used || !p2best)
                p2best = p; 
            // std::cout << "p2best (maybe) updated\n";
        }
        return p;
    }

    inline const mtmdd::StatsDD& best_entry() const {
        return *p2best;
    }

    inline const std::map<std::string, mtmdd::StatsDD>& get_cache()  const {
        return cache;
    }
};


class order_metric {
    int var; 
    double heu_value;
public:
    inline order_metric() 
    : var(-1), heu_value(-1) {}

    inline order_metric(const int var, const double hv) 
        : var(var), heu_value(hv) {}
    
    inline order_metric(const order_metric& o) 
        : var(o.var), heu_value(o.heu_value) {}
    
    //is the current metric better than x ? 
    inline bool is_better_than(const order_metric& x) const {
        return heu_value > x.heu_value; 
    }

    inline int get_var() const {  
        return var; 
    }
    inline double get_hval() const {   
        return heu_value;   
    }
    inline operator bool() const {  
        return var >= 0;    
    }
};


template<class T> 
class encoder {
    using keymap_t = T; 
    using valuemap_t = unsigned; 
    using map_t = std::map<keymap_t, valuemap_t>;

    map_t map; 
public:
    inline valuemap_t get(const T& x) {
        auto it = map.emplace(
            std::piecewise_construct, 
            std::forward_as_tuple(x), 
            std::forward_as_tuple(map.size())
        );
        return it.first->second;
    }

    inline size_t size() const {    
        return map.size(); 
    }
};


class contingency_table {
    uint64_t tot_observations = 0; 
public:
    contingency_table_t matrix; 
    contingency_row_t rowsums, colsums; 

    contingency_table(const int n_rows, const int n_cols) 
    : matrix(n_rows, contingency_row_t(n_cols, 0)), rowsums(n_rows), colsums(n_cols) {
    }

    inline void add(unsigned i, unsigned j, uint64_t val) {
        rowsums[i] += matrix[i][j] = val; 
        colsums[j] += val; 
        tot_observations += val; 
    }

    inline size_t nrows() const { return rowsums.size(); }
    inline size_t ncols() const { return colsums.size(); }
    inline uint64_t n() const { return tot_observations; }

    inline double chi2_stats() const {
        double chi2 = 0, ntot = tot_observations;
        const unsigned nr = nrows(), nc = ncols();
        

        for (int i = 0; i < nr; ++i) {
            for (int j = 0; j < nc; ++j) {
                double expected = rowsums[i] * colsums[j] / ntot;
                
                // std::cout << rowsums[i] << " * " << colsums[j] << " / " << ntot << " = " << expected << std::endl;  
                double num = matrix[i][j] - expected;
                chi2 += num * num / expected;
            }
        }  

        return chi2; 
    }

    inline double cramerV() const {
        double chi2 = chi2_stats();
        double minrc = (nrows() < ncols() ? nrows() : ncols()) - 1;
    
        double v =  sqrt(chi2 / (n() * minrc));
        // std::cout << "rows: " << show_vec(rowsums) << "\ncols: " << show_vec(colsums) << "\n";
        // std::cout << "chi2 : " << chi2 << " --- v: " << v << std::endl; 

        return v; 
    }

    inline void show() const {
        for (auto it = matrix.begin(); it != matrix.end(); ++it) {
            for (auto jt = it->begin(); jt != it->end(); ++jt)
                std::cout << *jt << " ";
            std::cout << "\n";
        }
    }
};

/*
class dd_collection {
    MEDDLY::domain *domain = nullptr; 
    MEDDLY::forest *forest = nullptr; 
    MEDDLY::forest::policies policy; 
    std::vector<MEDDLY::dd_edge> edges; 
    
    const int num_vars; 

    dd_collection(int num_vars) 
    : num_vars(num_vars), policy(false) {
        mtmdd::domain_bounds_t bounds(num_vars, -30); //XXX- define proper variable domains
        policy.setSparseStorage(); 
        domain = MEDDLY::createDomainBottomUp(bounds.data(), bounds.size());
        forest = domain->createForest(false, MEDDLY::forest::INTEGER, MEDDLY::forest::MULTI_TERMINAL, policy);
    }

    ~dd_collection() {
        edges.clear(); 
        MEDDLY::destroyForest(forest); 
        MEDDLY::destroyDomain(domain); 
    }
};  */

class heuristic {
    mtmdd::MultiterminalDecisionDiagram& index;
    long bufferlength = 1000000;  //non dovrebbe esserci 
    Buffer *buffer = nullptr; //non dovrebbe esserci 


/*
    void test_ordering(const mtmdd::var_order_t& ordering) {
        const mtmdd::StatsDD *pstat = heuristic::cache_ordering.get(ordering);
        mtmdd::StatsDD current_stats; 

        mtmdd::var_order_t dd_ordering;
        index.get_variable_ordering(dd_ordering); 


        if (!pstat) 
        {
            try {
                index.set_variable_ordering(ordering); 
                index.get_stats(current_stats);
                pstat = heuristic::cache_ordering.set(current_stats);
            } catch (MEDDLY::error& e) {
                std::cout << "MEDDLY EXPLODED DURING VARIABLE ORDERING :( with error " << e.getName() << std::endl; 
                std::cout << "desired ordering: " << show_vec(ordering) << "  -- prev ordering: " << show_vec(dd_ordering) << std::endl; 

                std::cout << "probably the forest is now broken... lets see..." << std::endl; 


                throw new MEDDLY::error(e);
            }
            // index.set_variable_ordering(ordering);   
            // index.get_stats(current_stats);
            // pstat = heuristic::cache_ordering.set(current_stats);
        }
    }*/

protected:
    mtmdd::var_order_t partial_ordering, unbounded_vars;
    int current_var; 
    int num_vars; 
    
    std::vector<long> terminals; //non dovrebbe esserci 
    // std::vector<dd_collection*> forests; 

    MEDDLY::domain *domain = nullptr; 
    MEDDLY::forest *forest = nullptr; 
    std::vector<MEDDLY::dd_edge> edges; 


    int init_forest(bool use_terminals = true);

    void clear_forest(); 

    MEDDLY::dd_edge& build_mtmdd(const int var, bool terminal_as_counts = false);

    void load_dd_data(const std::vector<int>& order);

public: 
    static cache_dd cache_ordering; 

    heuristic(mtmdd::MultiterminalDecisionDiagram& dd, bool keep_node_id) 
    : index(dd), terminals(bufferlength, 1) {
        dd.get_variable_ordering(unbounded_vars);
        unbounded_vars.erase(unbounded_vars.begin());       //remove starting zero 

        if (!keep_node_id) //assuming default ordering !!
            unbounded_vars.erase(unbounded_vars.end() - 1);     

        num_vars = unbounded_vars.size(); 

        //init cache 
        mtmdd::StatsDD stats; 
        index.get_stats(stats);
        heuristic::cache_ordering.set(stats);
    }

    const mtmdd::var_order_t& get();

    void metric_plot(std::map<combinations::combination, float>& metrics);

    inline order_metric get_metric(int var) {
        current_var = var; //update current variable 
        const MEDDLY::dd_edge& edge = build_mtmdd(current_var, true); 
        return compute_metric(edge);
    }

    virtual order_metric compute_metric(const MEDDLY::dd_edge& edge) = 0;
};


class entropy_heuristic: public heuristic {
    virtual order_metric compute_metric(const MEDDLY::dd_edge& edge); 
public:
    entropy_heuristic(mtmdd::MultiterminalDecisionDiagram& dd, bool keep_node_id = false) 
        : heuristic(dd, keep_node_id) {}
};



class correlation_heuristic: public heuristic {
    virtual order_metric compute_metric(const MEDDLY::dd_edge& edge); 
    
    void init_ordering(); 
public:
    correlation_heuristic(mtmdd::MultiterminalDecisionDiagram& dd, bool keep_node_id = false) 
        : heuristic(dd, keep_node_id) {}

    inline const mtmdd::var_order_t& get() {
        if (unbounded_vars.size() > 0)
            init_ordering(); 
        return heuristic::get();
    }
};
