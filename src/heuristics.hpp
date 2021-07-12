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

using contingency_row_t = std::vector<int>;
using contingency_table_t = std::vector< contingency_row_t >;


class order_metric; 
template<class T> class encoder;

class heuristic; 
class entropy_heuristic; 
class correlation_heuristic; 



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
    unsigned tot_observations = 0; 
public:
    contingency_table_t matrix; 
    contingency_row_t rowsums, colsums; 

    contingency_table(const int n_rows, const int n_cols) 
    : matrix(n_rows, contingency_row_t(n_cols, 0)), rowsums(n_rows), colsums(n_cols) {
    }

    inline void add(int i, int j, int val) {
        rowsums[i] += matrix[i][j] = val; 
        colsums[j] += val; 
        tot_observations += val; 
    }

    inline size_t nrows() const { return rowsums.size(); }
    inline size_t ncols() const { return colsums.size(); }
    inline unsigned n() const { return tot_observations; }

    double chi2_stats() const {
        double chi2 = 0;
        const unsigned nr = nrows(), nc = ncols(), ntot = n(); 

        // std::cout << "num samples : " << ntot << "\n"
        //     << "num rows: " << nr << ", num cols: " << nc << "\n"; 
        // for (auto x: rowsums)   std::cout << x << " "; std::cout << "\n"; 
        // for (auto x: colsums)   std::cout << x << " "; std::cout << "\n"; 

        for (int i = 0; i < nr; ++i) {
            for (int j = 0; j < nc; ++j) {
                double expected = static_cast<double>(rowsums[i] * colsums[j]) / ntot; 
                double num = matrix[i][j] - expected;
                chi2 += num * num / expected;
            }
        } 

        // std::cout << "final chi2 value: " << chi2 << std::endl;     

        return chi2; 
    }

    double cramerV() const {
        return pow(chi2_stats() / (n() * (std::min(nrows(), ncols()) - 1)), .5);
    }

    void show() const {
        for (auto it = matrix.begin(); it != matrix.end(); ++it) {
            for (auto jt = it->begin(); jt != it->end(); ++jt)
                std::cout << *jt << " ";
            std::cout << "\n";
        }
    }
};


class heuristic {
    mtmdd::MultiterminalDecisionDiagram& index;
    long bufferlength = 1000000; 
    Buffer *buffer = nullptr; 

protected:
    mtmdd::var_order_t partial_ordering, unbounded_vars;
    int current_var; 
    int num_vars; 
    
    std::vector<long> terminals; 
    MEDDLY::domain *domain = nullptr; 
    MEDDLY::forest *forest = nullptr; 
    std::vector<MEDDLY::dd_edge> edges; 

    int init_forest(bool use_terminals = true);

    void clear_forest(); 

    MEDDLY::dd_edge& build_mtmdd(const int var, bool terminal_as_counts = false);

    void load_dd_data(const std::vector<int>& order);

public: 
    heuristic(mtmdd::MultiterminalDecisionDiagram& dd, bool keep_node_id) 
    : index(dd), terminals(bufferlength, 1) {
        dd.get_variable_ordering(unbounded_vars);
        unbounded_vars.erase(unbounded_vars.begin());       //remove starting zero 
        num_vars = unbounded_vars.size(); 

        if (!keep_node_id) 
            unbounded_vars.erase(unbounded_vars.end() - 1); 
    }

    const mtmdd::var_order_t& get();

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
    entropy_heuristic(mtmdd::MultiterminalDecisionDiagram& dd) : heuristic(dd, false) {}
};



class correlation_heuristic: public heuristic {
    virtual order_metric compute_metric(const MEDDLY::dd_edge& edge); 
    
    void init_ordering(); 
public:
    correlation_heuristic(mtmdd::MultiterminalDecisionDiagram& dd) : heuristic(dd, false    ) {}

    inline const mtmdd::var_order_t& get() {
        init_ordering(); 
        return heuristic::get();
    }
};
