#include <algorithm>
#include <iostream>
#include <chrono>
#include <meddly.h>
#include <meddly_expert.h>

#include <iterator>
#include <vector>
#include <map>

#include "size_t.h"
#include "typedefs.h"

#include "Graph.h"
#include "GraphVisit.h"
#include "GraphPathListener.h"

#include "mtmdd.hpp"

#include "cxxopts.hpp"

/* given the path length L, test all possible variable orderings with L+1 variables (L+1)! */ 


size_t get_filesize(const std::string& filename); 

std::string format(long a, long b); 

void write_stats(std::ostream& out);
void write_stats(std::ostream& out, const mtmdd::var_order_t& varorder, const mtmdd::StatsDD& ddstats);


size_t build_dd(
        mtmdd::MultiterminalDecisionDiagram& dd,
        const std::string& db_filename, 
        const GraphsDB& db, 
        unsigned max_depth, 
        mtmdd::StatsDD& stats); 


void stringify_ordering(const mtmdd::var_order_t& vo, std::string& s);

int main(int argc, char** argv) {
    MEDDLY::initialize(MEDDLY::defaultInitializerList(NULL)); 

    cxxopts::Options options(argv[0], "GRAPES-DD"); 
    options.add_options()
        ("i, in", "graph database filename", cxxopts::value<std::string>())
        ("o, out", "out filename", cxxopts::value<std::string>())
        ("l, lp", "max pathlength", cxxopts::value<int>()->default_value("4"))
  //      ("d, direct", "are graph direct?", cxxopts::value<std::string>()->default_value("true")) 
  //      ("t, nthreads", "number of threads to use", cxxopts::value<int>()->default_value("8")) 
        // ("v, varorder", "mtmdd variable ordering", cxxopts::value<mtmdd::var_order_t>())
        ;

    cxxopts::ParseResult result = options.parse(argc, argv); 
    std::string graph_file, query_file, output_folder, output_file; 
    unsigned max_depth; 
    bool direct_graph;


    try {
        graph_file.assign(result["in"].as<std::string>()); 
        output_file.assign(result["out"].as<std::string>());
        max_depth = result["lp"].as<int>();; 

        std::string ext = ".gfd";
        direct_graph = graph_file.compare(graph_file.length() - ext.length(), ext.length(), ext) == 0; 
    } catch (std::domain_error& ex) {
        std::cout << "Missing IO parameters!\n";
        std::cout << options.help() << std::endl; 
        return 1; 
    }

    //load graphs from file 
    std::cout << "Loading graphs db..." << std::endl; 
    GraphsDB db(graph_file, direct_graph);  

    std::cout << "Building mtmdd from graphs db..." << std::endl; 
    mtmdd::MultiterminalDecisionDiagram dd(db, max_depth); 
    
    std::cout << "Starting variable ordering brute-forcing..." << std::endl;
    std::ofstream fo(output_file, std::ios::out);

    //start from the default variable ordering : 0,1,2,3,...L+1
    mtmdd::var_order_t variables;
    variables.resize(max_depth + 2); 
    std::iota(variables.begin(), variables.end(), 0);

    mtmdd::StatsDD best_of;
    mtmdd::var_order_t best_vars(variables); 

    dd.get_stats(best_of);

    write_stats(fo);
    write_stats(fo, best_vars, best_of);

    write_stats(std::cout);
    write_stats(std::cout, best_vars, best_of);     


    for (mtmdd::StatsDD statsdd; std::next_permutation(variables.begin()+1, variables.end());) {
        try {
            dd.set_variable_ordering(variables);
            dd.get_stats(statsdd); 

            write_stats(fo, variables, statsdd);

            if (statsdd.memory_used < best_of.memory_used) {
                best_vars = variables;
                best_of = statsdd;

                write_stats(std::cout, best_vars, best_of);
            }
        } 
        catch (std::exception const& ex) {
            // if case of a variable ordering change should fail
        }
    }

    std::cout << "Best variable ordering at all:\n";
    write_stats(std::cout);
    write_stats(std::cout, best_vars, best_of); 


    return 0;
}



size_t get_filesize(const std::string& filename) {
    std::ifstream f(filename, std::ios::binary);
    const auto begin = f.tellg();
    f.seekg(0, std::ios::end); 
    const auto end = f.tellg(); 

    return (end - begin);
}

size_t build_dd(
        mtmdd::MultiterminalDecisionDiagram& dd,
        const std::string& db_filename, 
        const GraphsDB& db, 
        unsigned max_depth, 
        mtmdd::StatsDD& stats) {

    dd.init(db, max_depth); 
    // dd.write(db_filename); 

    std::string index_filename = grapes2dd::get_dd_index_name(db_filename, max_depth);
    size_t size_dd = get_filesize(index_filename); 

    dd.get_stats(stats); 

    return size_dd; 
}

void stringify_ordering(const mtmdd::var_order_t& vo, std::string& s) {
    std::stringstream ss; 
    mtmdd::var_order_t::const_iterator it = vo.begin() + 1;

    ss << *it;
    while (++it != vo.end()) 
        ss << "," << *it;

    s = ss.str(); 
}

void write_stats(std::ostream& out) {
    constexpr const char sep[] = "\t";
    out << "ordering" << sep
        << "cardinality" << sep 
        << "num_nodes" << sep
        << "num_edges" << sep 
        << "memory_used" << sep
        << "peak_nodes" << sep
        << "peak_memory\n";
}

void write_stats(std::ostream& out, const mtmdd::var_order_t& varorder, const mtmdd::StatsDD& ddstats) {
    constexpr const char sep[] = "\t";
    std::string strvars;
    stringify_ordering(varorder, strvars); 

    out << strvars << sep
        << ddstats.cardinality << sep
        << ddstats.num_nodes << sep
        << ddstats.num_edges << sep 
        << ddstats.memory_used << sep 
        << ddstats.peak_nodes << sep 
        << ddstats.peak_memory << std::endl; 
}