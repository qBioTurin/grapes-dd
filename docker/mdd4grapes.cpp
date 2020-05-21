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
#include <fstream>
#include <cctype>
#include <string>
#include <numeric>
#include "indexing/cxxopts.hpp"

enum class Mode {
    helper, 
    indexing,
    grapes
}; 


Mode get_mode(const std::string& mode) {
    std::map<std::string, Mode> mode_map{
        {"grapes", Mode::grapes}, 
        {"grapes_dd", Mode::indexing}
    }; 
    auto it = mode_map.find(mode); 
    
    return it != mode_map.end() ? it->second : Mode::helper; 
}

std::string basename(const std::string& filename) {
    return filename.substr(filename.rfind("/") + 1);
}

void helper(char* arg0) {
    std::cout << "Usage: " << arg0 << " [grapes|grapes_dd] [args...]" << std::endl; 
}




int main(int argc, char* argv[]) {
    cxxopts::Options options(argv[0], "GRAPES-DD");
    Mode mode = Mode::helper; 
    std::string input_file, query_file; 
    int depth = 4; 
    std::string direct_flag; 
    int nthreads; 
    std::vector<std::string> command_tokens; 

    try {
        std::string arg1(argv[1]); 
        mode = get_mode(arg1);         
    }
    catch (std::logic_error& e) {
        helper(argv[0]); 
        return 0; 
    }

    //parsing GRAPES arguments 
    if (mode == Mode::grapes) {
        options.add_options()
            ("i, db_file", "File containing the collection of graphs to be indexed", cxxopts::value<std::string>())
            ("q, query", "File containing the query graph to be searched", cxxopts::value<std::string>())
            ("d, direct", "Are graphs directed?", cxxopts::value<std::string>()->default_value("true"))
            ("t, threads", "Number of threads to use", cxxopts::value<int>()->default_value("1"))
            ("l, lp", "Max path length", cxxopts::value<int>());
        auto result = options.parse(argc, argv); 

        try {
            input_file.assign("/data/db_file/" + basename(result["db_file"].as<std::string>())); 

            direct_flag.assign(result["direct"].as<std::string>().compare("true") == 0 ? "-gfd" : "-gfu"); 
            nthreads = result["threads"].as<int>(); 
            depth = result["lp"].as<int>(); 

            if (result["query"].count() == 1) {
                query_file.assign("/data/query_file/" + basename(result["query"].as<std::string>())); 
            }
        } catch (std::domain_error& ex) {
            std::cout << "Missing mandatory parameters!\n";
            std::cout << options.help() << std::endl; 
            return 1; 
        }

        if (query_file.empty()) {
            //default grapes command: build the index 
            command_tokens = {
                "grapes",                   //calling grapes
                std::to_string(nthreads),   //using n threads
                "-b",                       //to build the index 
                direct_flag,                //in (un)directed mode
                input_file,                 //this file is the index   
                "-lp", std::to_string(depth)//this is the max path length 
            }; 
        } else {
            command_tokens = {
                "grapes",                  //calling grapes
                std::to_string(nthreads),   //using n threads
                "-f",                     //to match a query 
                direct_flag,                //in (un)directed mode 
                input_file,                 //this file is the index 
                query_file,                 //and this is the query graph 
                "-no",                      //show no results
                "-lp", std::to_string(depth)//this is the max path length 
            }; 
        }
    } 
    //parsing decision diagram indexing arguments
    else if (mode == Mode::indexing) {
        options.add_options()
            ("i, db_file", "File containing the collection of graphs to be indexed", cxxopts::value<std::string>())
            ("q, query", "File containing the query graph to be searched", cxxopts::value<std::string>())
            ("l, lp", "Max path length", cxxopts::value<int>())
            ("d, direct", "Are graphs directed?", cxxopts::value<std::string>()->default_value("true"))
            ("t, nthreads", "Number of threads to use", cxxopts::value<int>()->default_value("1")) //not used yet...
            ("b, bsize", "Size buffer used during mtmdd loading", cxxopts::value<int>()->default_value("10000"))
            ("log", "Filename where to save mtmdd stats", cxxopts::value<std::string>()->default_value("indexing_results"));
        ;
        auto result = options.parse(argc, argv); 

        std::string logfilename; 
        int buffersize; 

        try {
            input_file.assign("/data/db_file/" + basename(result["db_file"].as<std::string>())); 
            depth = result["lp"].as<int>(); 
            direct_flag = result["direct"].as<std::string>(); 
            nthreads = result["nthreads"].as<int>(); 
            buffersize = result["bsize"].as<int>(); 
            logfilename.assign(result["log"].as<std::string>()); 

            if (result["query"].count() == 1) {
                //todo - add possibility to pass multiple query files 
                query_file.assign("/data/query_file/" + basename(result["query"].as<std::string>())); 
            }

        } catch (std::domain_error& ex) {
            std::cout << "Missing mandatory parameters!\n";
            std::cout << options.help() << std::endl; 
            return 1; 
        }

        command_tokens = {
            "grapes_dd", 
            "--in", input_file, 
            "--lp", std::to_string(depth), 
            "--direct", direct_flag, 
            "--nthreads", std::to_string(nthreads), 
            "--bsize", std::to_string(buffersize), 
            "--log", logfilename
        }; 

        if (!query_file.empty()) {
            command_tokens.push_back("--query"); 
            command_tokens.push_back(query_file); 
        }
    } else {
        helper(argv[0]); 
        return 0; 
    }


    std::string command; 
    for (const std::string& s: command_tokens)
        command += s + " "; 

    std::string memory_file = "/data/db_file/memory_peak.stat"; 
    std::ofstream fo(memory_file, std::ios_base::out | std::ios_base::app);
    fo  << (mode == Mode::indexing ? "mtmdd" : "grapes") << (query_file.empty() ? " " : "_query ")
        << input_file << " " 
        << depth 
        << "\n/usr/bin/time output: "; 
    fo.close(); 

    command = "/usr/bin/time -f '%M' " + command + " 2>> " + memory_file;

    std::cout 
        << "Executing the following command:\n" 
        << command << std::endl; 
    
    system(command.c_str()); 
}