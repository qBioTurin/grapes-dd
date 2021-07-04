#include "mtmdd.hpp"

#include <meddly.h>
#include <meddly_expert.h>
#include <iterator>
#include <set>
#include <vector>
#include <numeric>

#include "mtmdd.hpp"
#include "cxxopts.hpp"

class dd_heuristic; 
class order_metric; 

class order_metric {
    int var; 
    long long int heu_value;
public:
    order_metric() 
    : var(-1), heu_value(0) {}

    order_metric(const int var, const long long int hv) 
        : var(var), heu_value(hv) {}
    
    bool operator>(const order_metric& x) const {
        return heu_value > x.heu_value;
    }

    inline int get_var() {  return var; }
    inline long long int get_hval() {   return heu_value;   }
};

class dd_heuristic {
    std::vector<int> partial_ordering;
    std::vector<int> unbounded_vars;
    int num_vars; 
    mtmdd::MultiterminalDecisionDiagram& dd;

    Buffer *buffer = nullptr; 
    MEDDLY::domain *domain = nullptr; 
    MEDDLY::forest *forest = nullptr; 
    std::vector<MEDDLY::dd_edge> edges; 

    double compute_entropy(MEDDLY::dd_edge& dd);

public: 
    dd_heuristic(mtmdd::MultiterminalDecisionDiagram& dd) : dd(dd) {
        dd.get_variable_ordering(unbounded_vars);
        unbounded_vars.erase(unbounded_vars.begin());       //remove starting zero 
        num_vars = unbounded_vars.size(); 
    }


    double entropy_metric(const int var); 
    void entropy_heuristic();


}; 

