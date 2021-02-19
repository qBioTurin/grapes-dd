/*
Copyright (c) 2020

GRAPES is provided under the terms of The MIT License (MIT):

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <algorithm>
#include <iostream>
#include <chrono>
#include <meddly.h>
#include <meddly_expert.h>

#include "mtdds.hpp"
#include "cxxopts.hpp"

using namespace mtdds; 


void create_logfile_indexing(const std::string& logname); 

void create_logfile_matching(const std::string& logname); 

void add_to_indexing_logfile(const std::string& logname, const std::string& db_filename, 
    const std::vector<long>& stats, const std::vector<double>& cpu_times);

void add_to_matching_logfile(const std::string& logname, const std::string& db_filename, 
    const std::string& query_filename, const std::vector<long>& stats, const std::vector<double>& cpu_times);

inline std::string basename(std::string filename) { 
    return filename.substr(filename.rfind("/") + 1);
}

inline std::string dirname(std::string filename) {
    return filename.substr(0, filename.rfind("/")); 
}

int main(int argc, char** argv) {
    MEDDLY::initialize(MEDDLY::defaultInitializerList(NULL)); 

    cxxopts::Options options(argv[0], "GRAPES-DD"); 
    options.add_options()
        ("i, in", "graph database filename", cxxopts::value<std::string>())
        ("q, query", "query graph to search in the db", cxxopts::value<std::string>())
        ("l, lp", "max pathlength", cxxopts::value<int>()->default_value("4"))
        ("d, direct", "are graph direct?", cxxopts::value<std::string>()->default_value("true")) 
        ("t, nthreads", "number of threads to use", cxxopts::value<int>()->default_value("8")) 
        ("b, bsize", "size buffer to load data into mtmdd", cxxopts::value<int>()->default_value("10000"))
        ("log", "log filename", cxxopts::value<std::string>()->default_value("indexing_results"));

    cxxopts::ParseResult result = options.parse(argc, argv); 
    std::string graph_file, query_file, output_folder, log_file; 
    int max_depth, nthreads, buffersize;
    bool direct_graph; 


    try {
        graph_file.assign(result["in"].as<std::string>()); 
        output_folder.assign(graph_file.substr(0, graph_file.find_last_of("/"))); 
        max_depth = result["lp"].as<int>();
        buffersize = result["bsize"].as<int>();
        nthreads = result["nthreads"].as<int>();
        direct_graph = result["direct"].as<std::string>().compare("true") == 0; 
        log_file.assign(result["log"].as<std::string>());

        if (result["query"].count() > 0) {
            query_file.assign(result["query"].as<std::string>()); 

            if (!grapes2mtdds::dd_already_indexed(graph_file, max_depth)) {
                std::cerr << "You have to index the graph db before to perform query matching!" << std::endl; 
                return 1; 
            }
        }

    } catch (std::domain_error& ex) {
        std::cout << "Missing IO parameters!\n";
        std::cout << options.help() << std::endl; 
        return 1; 
    }
    
    if (query_file.empty()) {
        // INDEX BUILDING 
        time_point start_build, end_build;
        time_point start_saving, end_saving; 
        double time_build = 0, time_saving = 0; 

        std::cout
            << "\t\t============== BUILD ================\n\n"
            << "MAX LP depth: " << max_depth << "\n"
            << "Input database file: " << graph_file << "\n\n";

        start_build = std::chrono::_V2::steady_clock::now(); 
        mtdds::MultiterminalDecisionDiagram mtmdd_index(graph_file, max_depth, direct_graph, buffersize); 
        end_build = std::chrono::_V2::steady_clock::now(); 
        time_build = get_time_interval(end_build, start_build); 

        start_saving = std::chrono::_V2::steady_clock::now(); 
        mtmdd_index.write(graph_file); 
        end_saving = std::chrono::_V2::steady_clock::now(); 
        time_saving = get_time_interval(end_saving, start_saving); 

        //log stats on file  
        MtddStats stats; 
        mtmdd_index.get_stats(stats); 

        std::vector<long> current_stats{
            stats.num_vars, stats.num_graphs, stats.num_labels, static_cast<long>(direct_graph),
            stats.num_nodes, stats.peak_nodes, 
            stats.num_edges,
            stats.memory_used, stats.peak_memory, 
            stats.cardinality
        };
        std::vector<double> cpu_times {
            time_build, time_saving, time_build + time_saving  
        }; 

        log_file = dirname(graph_file) + "/" + log_file; 
        add_to_indexing_logfile(log_file, basename(graph_file), current_stats, cpu_times); 

        std::cout
            << "Number of graphs inside the database: " << mtmdd_index.num_indexed_graphs() << "\n\n"
            << "Num. paths encoded in MTMDD: " << stats.cardinality << "\n"
            << "Memory required: " << stats.memory_used << " (peak = " << stats.peak_memory << ")\n"
            << "Num nodes in MTMDD: " << stats.num_nodes << " (peak = " << stats.peak_nodes << ")\n"
            << "Num edges: " << stats.num_edges << "\n"
            << "Time for build database index: " << time_build << "\n"
            << "Time for save index on file: " << time_saving << "\n"
            << "Total time: " << time_build + time_saving  << std::endl; 
    } 
    else {
        // QUERY MATCHING 
        mtdds::MultiterminalDecisionDiagram mtmdd_index; 
        time_point start_loading, end_loading; 
        std::vector<double> stages_times;  
        double total_time = 0; 

        std::cout << "\t\t============== GRAPES-DD QUERY MATCHING ================\n\n"
            << "Input database: " << graph_file << "\n"
            << "Input query graph: " << query_file << "\n"
            << "Number of threads: " << nthreads << "\n"
            << "MAX LP depth: " << max_depth << "\n";

        start_loading = std::chrono::_V2::steady_clock::now(); 
        mtmdd_index.read(graph_file, max_depth); 
        end_loading = std::chrono::_V2::steady_clock::now(); 
        stages_times.push_back(get_time_interval(end_loading, start_loading)); 

        std::vector<GraphMatch> matched_graphs(mtmdd_index.match(query_file, stages_times)); 
        std::map<std::string, double> matching_stats;

        mtdds::graph_find(graph_file,
            query_file, 
            direct_graph, 
            nthreads, 
            mtmdd_index.labelMapping,
            matched_graphs, 
            matching_stats
        );

        for (auto it = stages_times.begin(); it != stages_times.end(); ++it)
            total_time += *it; 

        total_time += matching_stats["tot_matching_time"]; 
        
        stages_times.push_back(total_time); 

        std::cout 
            << "Number of graphs inside the database: " << mtmdd_index.num_graphs_in_db << "\n"
            << "Number of candidate graphs: " << matching_stats["n_cand_graphs"] << "\n"
            << "Number of connected components by filtering: "<< matching_stats["n_cocos"] << "\n"
            << "Number of matching graphs: "<< matching_stats["n_matching_g"] <<"\n"
            << "Number of found matches: "<< matching_stats["n_found_m"] <<"\n"
            << "Time to load the index: " << stages_times.at(0) << "\n"
            << "Time to query indexing: " << stages_times.at(1) << "\n"
            << "Time to extract candidate paths: " << stages_times.at(2) << "\n"
            << "Filtering time: " << stages_times.at(3) << "\n"
            << "DB load time: " << matching_stats["load_db_time"] << "\n"
            << "DB's decomposing time: " << matching_stats["decompose_time"] << "\n"
            << "Matching time: " << matching_stats["matching_time"] << "\n"
            << "Total time: " << total_time  << "\n"; 

        MtddStats stats; 
        mtmdd_index.get_stats(stats); 

        std::vector<long> current_stats {
            stats.num_vars, stats.num_graphs, stats.num_labels, static_cast<long>(direct_graph), 
            static_cast<long>(matched_graphs.size())
        }; 
        
        log_file = dirname(graph_file) + "/" + log_file; 
        add_to_matching_logfile(
            log_file, 
            basename(graph_file), 
            basename(query_file),
            current_stats, 
            stages_times
        ); 
    }

    return 0; 
}



void create_logfile_indexing(const std::string& logname) {
    std::string filename(logname + ".dd_stats");

    if (!std::ifstream(filename).good()) {
        std::ofstream f(filename, std::ios::out); 

        std::vector<std::string> header{
            "db", "depth", "ngraphs", "nlabels", "direct",
            "nnodes", "pnodes", "nedges", 
            "mem", "peak_mem", "nelems", 
            "build_t", "save_t", "total_t"
        }; 

        f << header.at(0); 
        for (auto it = header.begin() + 1; it != header.end(); ++it)
            f << "\t" << *it; 
        f << std::endl; 
    }
}

void create_logfile_matching(const std::string& logname) {
    std::string filename(logname + ".dd_match"); 

    if (!std::ifstream(filename).good()) {
        std::ofstream f(filename, std::ios::out); 

        std::vector<std::string> header{
            "db", "query", "depth", "ngraphs", "nlabels", "direct", 
            "nmatched_g",
            "load_t", "q_index_t", "q_match_f", "q_filter_t", "total_t" 
        }; 

        f << header.at(0); 
        for (auto it = header.begin() + 1; it != header.end(); ++it)
            f << "\t" << *it; 
        f << std::endl; 
    }   
}

void add_to_indexing_logfile(const std::string& logname, const std::string& db_filename, const std::vector<long>& stats, const std::vector<double>& cpu_times) {
    create_logfile_indexing(logname); 
    
    std::string filename(logname + ".dd_stats"); 
    std::ofstream f(filename, std::ios::out | std::ios::app); 

    f << db_filename; 
    for (const long& stat: stats)       f << "\t" << stat; 
    for (const double& stat: cpu_times) f << "\t" << stat; 
    f << std::endl; 
}

void add_to_matching_logfile(
    const std::string& logname, 
    const std::string& db_filename, 
    const std::string& query_filename,
    const std::vector<long>& stats, 
    const std::vector<double>& cpu_times) {

    create_logfile_matching(logname); 

    std::string filename(logname +  ".dd_match"); 
    std::ofstream f(filename, std::ios::out | std::ios::app); 

    f << db_filename << "\t" << query_filename; 
    for (const long& stat: stats)       f << "\t" << stat;
    for (const double& stat: cpu_times) f << "\t" << stat;
    f << std::endl;
}