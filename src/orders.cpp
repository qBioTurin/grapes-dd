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




size_t get_filesize(const std::string& filename); 

std::string format(long a, long b); 

size_t build_dd(
        mtmdd::MultiterminalDecisionDiagram& dd,
        const std::string& db_filename, 
        const GraphsDB& db, 
        unsigned max_depth, 
        mtmdd::StatsDD& stats); 


int main(int argc, char** argv) {
    MEDDLY::initialize(MEDDLY::defaultInitializerList(NULL)); 

    cxxopts::Options options(argv[0], "GRAPES-DD"); 
    options.add_options()
        ("i, in", "graph database filename", cxxopts::value<std::string>())
        // ("o, out", "out filename", cxxopts::value<std::string>())
        ("l, lp", "max pathlength", cxxopts::value<int>()->default_value("4"))
  //      ("d, direct", "are graph direct?", cxxopts::value<std::string>()->default_value("true")) 
  //      ("t, nthreads", "number of threads to use", cxxopts::value<int>()->default_value("8")) 
        ("v, varorder", "mtmdd variable ordering", cxxopts::value<mtmdd::var_order_t>())
        ;

    cxxopts::ParseResult result = options.parse(argc, argv); 
    std::string graph_file, query_file, output_folder, output_file; 
    unsigned max_depth; 
    bool direct_graph;

    mtmdd::var_order_t variables;


    try {
        graph_file.assign(result["in"].as<std::string>()); 
        // output_file.assign(result["out"].as<std::string>());
        max_depth = result["lp"].as<int>();; 

        std::string ext = ".gfd";
        direct_graph = graph_file.compare(graph_file.length() - ext.length(), ext.length(), ext) == 0; 

        if (result["varorder"].count() > 0) {
            variables = result["varorder"].as<mtmdd::var_order_t>();
            
            if (variables.size() > max_depth + 1) {
                throw std::logic_error("Invalid variable ordering --- too much variables w.r.t max_depth value."); 
            } else if (variables.size() < max_depth + 1) {
                throw std::logic_error("Invalid variable ordering --- too few variables w.r.t max_depth value."); 
            } else {
                variables.insert(variables.begin(), 0); 
            }
        } else {
            //impose default order 
            variables.resize(max_depth + 1); 
            std::iota(variables.begin(), variables.end(), 1);
        }
    } catch (std::domain_error& ex) {
        std::cout << "Missing IO parameters!\n";
        std::cout << options.help() << std::endl; 
        return 1; 
    }

    //load graphs from file 
    std::cout << "Loading graphs db..." << std::endl; 
    GraphsDB db(graph_file, direct_graph);  

    mtmdd::MultiterminalDecisionDiagram dd; 
    mtmdd::StatsDD stats_order, stats_default;

    std::cout << "Building mtmdd from graphs db..." << std::endl; 
    size_t size_dd = build_dd(dd, graph_file, db, max_depth, stats_default); 

    dd.writePicture("pdf/initial");

    dd.impose_order(variables); 
    dd.get_stats(stats_order); 

    dd.write("pdf/gemitaiz");
    size_t size_dd2 = get_filesize(grapes2dd::get_dd_index_name("pdf/gemitaiz", max_depth));

    dd.writePicture("pdf/resulting");


    std::cout << 
        "Num nodes: " << format(stats_order.num_nodes, stats_default.num_nodes) <<
        "Peak nodes: " << format(stats_order.peak_nodes, stats_default.peak_nodes) <<
        "Num edges: " << format(stats_order.num_edges, stats_default.num_edges) <<
        "Memory used: " << format(stats_order.memory_used, stats_default.memory_used) <<
        "Peak memory: " << format(stats_order.peak_memory, stats_default.peak_memory) <<
        "Index size: " << format(size_dd2, size_dd) << 
        "Cardinality: " << format(stats_order.cardinality, stats_default.cardinality) << std::endl; 

    return 0; 
}



size_t get_filesize(const std::string& filename) {
    std::ifstream f(filename, std::ios::binary);
    const auto begin = f.tellg();
    f.seekg(0, std::ios::end); 
    const auto end = f.tellg(); 

    return (end - begin);
}

std::string format(long a, long b) {
    std::stringstream ss; 
    double rate = static_cast<double>(a) / static_cast<double>(b); 

    ss << a << " (" << b << ") => " << rate << "\n"; 
    return ss.str(); 
}

size_t build_dd(
        mtmdd::MultiterminalDecisionDiagram& dd,
        const std::string& db_filename, 
        const GraphsDB& db, 
        unsigned max_depth, 
        mtmdd::StatsDD& stats) {

    dd.init(db, max_depth); 
    dd.write(db_filename); 

    std::string index_filename = grapes2dd::get_dd_index_name(db_filename, max_depth);
    size_t size_dd = get_filesize(index_filename); 

    dd.get_stats(stats); 

    return size_dd; 
}