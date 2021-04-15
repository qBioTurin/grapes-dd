#include <algorithm>
#include <iostream>
#include <chrono>
#include <meddly.h>
#include <meddly_expert.h>

#include <iterator>
#include <vector>
#include <map>

#include "GraphPathListener.h"
#include "GraphVisit.h"
#include "size_t.h"

#include "mtmdd.hpp"

#include "cxxopts.hpp"

int main(int argc, char** argv) {
    MEDDLY::initialize(MEDDLY::defaultInitializerList(NULL)); 

    cxxopts::Options options(argv[0], "GRAPES-DD"); 
    options.add_options()
        ("i, in", "graph database filename", cxxopts::value<std::string>())
        ("o, out", "out filename", cxxopts::value<std::string>())
        ("l, lp", "max pathlength", cxxopts::value<int>()->default_value("4"))
        ("d, direct", "are graph direct?", cxxopts::value<std::string>()->default_value("true")) 
        ("t, nthreads", "number of threads to use", cxxopts::value<int>()->default_value("8")) 
        ("b, bsize", "size buffer to load data into mtmdd", cxxopts::value<int>()->default_value("10000"));

    cxxopts::ParseResult result = options.parse(argc, argv); 
    std::string graph_file, query_file, output_folder, output_file; 
    int nthreads, buffersize;
    unsigned max_depth; 
    bool direct_graph; 


    try {
        graph_file.assign(result["in"].as<std::string>()); 
//        output_folder.assign(graph_file.substr(0, graph_file.find_last_of("/"))); 
        output_file.assign(result["out"].as<std::string>());
        max_depth = result["lp"].as<int>();
        buffersize = result["bsize"].as<int>();
        nthreads = result["nthreads"].as<int>();
        direct_graph = result["direct"].as<std::string>().compare("true") == 0; 

    } catch (std::domain_error& ex) {
        std::cout << "Missing IO parameters!\n";
        std::cout << options.help() << std::endl; 
        return 1; 
    }

    //load graphs from file 
    GraphsDB db(graph_file, direct_graph); 
    std::queue<GRAPESLib::Graph>& graphs_queue = db.graphs_queue; 

    //init empty mtmdd with its own domain 
    mtmdd::domain_bounds_t bounds(max_depth + 1, db.labelMap.size() + 1);
    bounds.at(max_depth) = MEDDLY::DONT_CARE;  //unknown number of nodes
    mtmdd::MultiterminalDecisionDiagram dd(bounds); 

    Buffer dd_buffer(buffersize, bounds.size() + 1, true); 
    mtmdd::MtmddLoaderListener mlistener(dd, dd_buffer); 

    while (!graphs_queue.empty()) {
        GRAPESLib::Graph& current_graph = graphs_queue.front(); 

        //grouping graph's nodes w.r.t label and sorting them w.r.t out degree
        std::sort(
            current_graph.nodes, 
            current_graph.nodes + current_graph.nodes_count, 
            [](const GRAPESLib::GNode& a, const GRAPESLib::GNode& b) -> bool {
                return a.label != b.label ? 
                    a.label > b.label : 
                    a.out_neighbors.size() > b.out_neighbors.size();
            }
        ); 

        GRAPESLib::OCPTree partial_index; 
        GRAPESLib::AllPathListener plistener(&partial_index, current_graph.id); 
        GRAPESLib::DFSGraphVisitor gvisitor(plistener); 

        //starting dfs visit from the first N nodes with higher degree 
        int nodes_count = current_graph.nodes_count; 

        for (int i = 0; i < nodes_count; ) {
            node_label_t curr_label = current_graph.nodes[i].label; 
            int dfs_counter = 0, max_dfs_nodes = 5; 

            do {
                if (++dfs_counter <= max_dfs_nodes) {
                    gvisitor.run(current_graph, current_graph.nodes[i], max_depth); 
                }
            } while (++i < nodes_count && curr_label == current_graph.nodes[i].label);
        }

        //there is no need to store stuff in the dd ... 
        partial_index.visit(mlistener); 

        graphs_queue.pop(); 
    }

    //flush the remaining data from buffer to mtmdd
    if (dd_buffer.num_elements() > 0) {
        dd.insert(dd_buffer); 
    }

    dd.save_data(output_file);

    return 0; 
}