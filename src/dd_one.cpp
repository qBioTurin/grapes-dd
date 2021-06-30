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


class dd_ordering; 
class ddstats;
class ddtester;


using strvector = std::vector<std::string>;
using varorder_vector = std::vector<dd_ordering>;
using varorderdict = std::map<std::string, varorder_vector>;
using statsvector = std::vector<ddstats>;


class dd_ordering {
public:
    const std::string name; 
    const mtmdd::var_order_t ordering; 

    dd_ordering(const std::string& name, const mtmdd::var_order_t& ordering) : name(name), ordering(ordering) {}
};

class ddstats {
public:
    const std::string ddname;
    const unsigned num_nodes, num_edges, memory_used;
    const unsigned peak_nodes, peak_memory, cardinality;
    dd_ordering ordering; 

    ddstats(const std::string& name, const mtmdd::StatsDD& stats, const dd_ordering& ordering) 
        :   ddname(name), num_nodes(stats.num_nodes), num_edges(stats.num_edges), memory_used(stats.memory_used), 
            peak_nodes(stats.peak_nodes), peak_memory(stats.peak_memory), cardinality(stats.cardinality), 
            ordering(ordering) {
    }

};

class ddtester {
    varorderdict dictionary; 
    statsvector 
        defaultorderstats, //dd stats using default variable ordering 
        chosenorderstats,  //dd stats using the variable ordering obtained from heuristic
        reverseorderstats;//, //dd stats using the reverse variable ordering obtained from heuristics 
        //almostreverseorderstats; //dd stats using the reverse variable ordering BUT keeping node id information in the DD root 

    ddstats extract_ddstats(const std::string& name, const dd_ordering& ordering, const mtmdd::MultiterminalDecisionDiagram& dd) {
        mtmdd::StatsDD s;
        dd_ordering o(ordering.name, dd.v_order->var_order);
        dd.get_stats(s);

        return ddstats(name, s, o);
    }

    void save_header(std::ofstream& f) {
        std::vector<std::string> header {
            "name", "cardinality", "n_nodes", "n_edges", 
            "memory_used", "peak_nodes", "peak_memory", "tag", "ordering"};
        f << header.at(0);
        for (auto it = header.begin() + 1; it != header.end(); ++it)    f << "\t" << *it;
        f << "\n";
    }

    void save_record(std::ofstream& f, const ddstats& stats, const std::string& tag) {
        mtmdd::var_order_t::const_iterator 
            it = stats.ordering.ordering.begin(), 
            last_but_one = stats.ordering.ordering.end() - 1; 
        std::stringstream ss;
        
        while (++it != last_but_one) 
            ss << *it << ",";
        ss << *it; 

        f   << stats.ddname << "\t" 
            << stats.cardinality << "\t"
            << stats.num_nodes << "\t"
            << stats.num_edges << "\t"
            << stats.memory_used << "\t"
            << stats.peak_nodes << "\t"
            << stats.peak_memory << "\t"
            << tag << "\t"
            << ss.str() << "\n";
    }
public: 
    ddtester(const std::string& varorder_filename);

    ddtester(const ddtester& ddt) : dictionary(ddt.dictionary) {
    }

    void get_ddstats(const std::string& dd_name, const std::string& dd_filename);

    void save_report(const std::string& outfolder, const std::string& outfilename);

    void get_datasets(strvector& v) {
        v.clear();
        for (const auto& pair: dictionary)
            v.emplace_back(pair.first); 
    }    
};

inline bool file_really_exists(const std::string& input_file) {
    return std::ifstream(input_file, std::ios::in).good(); 
}



int main(int argc, char** argv) {
    MEDDLY::initialize(MEDDLY::defaultInitializerList(NULL)); 

    const std::string 
        suffix_complete = "_all_paths.dd_txt", 
        suffix_no_occ = "_all_paths_no_occ.dd_txt";
    const std::string
        extension_direct = ".gfd", 
        extension_undirect = ".gfu"; 


    cxxopts::Options options(argv[0], ""); 
    options.add_options()
        ("i, in", "input folder", cxxopts::value<std::string>())
        ("o, out", "out folder", cxxopts::value<std::string>())
        ("d, datasets", "list of dataset names in input folder", cxxopts::value<strvector>())
        ("v, varorders", "variable orders file", cxxopts::value<std::string>())
        ("l, lp", "max path length", cxxopts::value<int>())
        ;

    cxxopts::ParseResult result = options.parse(argc, argv); 
    std::string input_folder, output_folder, varorder_filename;
    int path_length = 4;
    strvector datasets; 
    varorderdict dictionary; 
    mtmdd::var_order_t ordering;

    try {
        if (result["datasets"].count() > 0)
            datasets = result["datasets"].as<strvector>();
        if (result["in"].count() > 0)
            input_folder.assign(result["in"].as<std::string>());
        if (result["out"].count() > 0)
            output_folder.assign(result["out"].as<std::string>());
        if (result["varorders"].count() > 0)
            varorder_filename.assign(result["varorders"].as<std::string>());
        if (result["lp"].count() > 0) 
            path_length = result["lp"].as<int>();
    }
    catch(const std::exception& e) {
        std::cerr << "exploded... but why? Answer: " << e.what() << '\n';
        return 1; 
    }
    
    if (input_folder.empty()) {
        std::cerr << "Please insert input file/folder!" << std::endl;
        return 2;
    }

    if (varorder_filename.empty()) {
        /* no variable ordering provided -   
         * assume the input string is a filename - index the content (assume that is a graph db, otherwise gg)
         * and save raw paths in a file called DBNAME + SUFFIX_COMPLETE         */

        std::string& input_file = input_folder; 
        std::size_t input_length = input_file.length(), direct_length = extension_direct.length(), undirect_length = extension_undirect.length();
        bool direct_ = input_file.compare(input_length - direct_length, direct_length, std::string(extension_direct)) == 0;
        bool undirect_ = input_file.compare(input_length - undirect_length, undirect_length, std::string(extension_undirect)) == 0;

        if (!direct_ && !undirect_) {
            std::cerr << "Unrecognized input filetype: " << input_file << std::endl; 
            return 3; 
        }
        if (file_really_exists(input_file)) {
            unsigned buffersize = 1000000;

            mtmdd::MultiterminalDecisionDiagram dd(input_file, path_length, direct_, buffersize);
            std::string raw_filename = input_file.substr(0, input_file.length() - 4); //remove extension from filename 
            raw_filename += suffix_complete; 
            dd.save_data(raw_filename); 

            std::cout << "Output file produced: " << raw_filename << std::endl; 
        }
    } 
    else {
        ddtester tester_complete(varorder_filename);
        // ddtester tester_just1(tester_complete);
        
        if (output_folder.empty()) {
            std::cerr << "Please insert the output folder where to save stats information about variable ordering" << std::endl;
            return 2;
        }


        if (datasets.empty()) {
            std::cout << "Getting all datasets from varorder file" << std::endl; 
            tester_complete.get_datasets(datasets); 
        }


        for (const std::string& name: datasets) {
            std::cout << "Dataset name: " << name << std::endl; 

            std::string prefix_filename = input_folder + "/" + name;

            std::cout << "prefix: " << prefix_filename << std::endl;

            tester_complete.get_ddstats(name, prefix_filename + suffix_complete);
            // tester_just1.get_ddstats(name, prefix_filename + suffix_no_occ);
        }

        tester_complete.save_report(output_folder, "report_full.tsv");
        // tester_just1.save_report(output_folder, "report_wocounts.tsv");
    }

    return 0; 
}



ddtester::ddtester(const std::string& varorder_filename) {
    Parser parser(",");
    std::ifstream fi(varorder_filename, std::ios::in); 

    while (fi.good()) {
        std::string folder, input_file, db_name, variables, heuristic; 
        mtmdd::var_order_t ordering; 

        fi >> folder >> input_file >> db_name >> variables >> heuristic; 
        std::cout << "current h: " << heuristic << std::endl; 

        try {
            parser.set_string(variables.c_str()); 

            while (true) 
                ordering.push_back(parser.parseint()); 
        } catch (std::exception& e) {}
        
        if (!ordering.empty()) {
            ordering.insert(ordering.begin(), 0); 
            auto ret = dictionary.emplace(
                std::piecewise_construct, 
                std::forward_as_tuple(db_name), 
                std::forward_as_tuple()
            ); 
            //get value from dictionary
            varorder_vector& v(ret.first->second); 
            v.emplace_back(heuristic, ordering);
            // ret.first->second.emplace(ordering, heuristic);
            // ret.first->second.emplace()
            
            // std::cout << "emplace " << db_name << ": " << ret.second << std::endl; 
        //    dictionary.emplace(db_name, ordering);
        } 
    }
}




void ddtester::get_ddstats(const std::string& dd_name, const std::string& dd_filename) {
    mtmdd::MultiterminalDecisionDiagram dd; 
    mtmdd::StatsDD current_stats; 

    if (file_really_exists(dd_filename)) {
        varorderdict::iterator it; 
        
        if ((it = dictionary.find(dd_name)) != dictionary.end()) {
            std::cout << "Processing " << dd_name << std::endl; 
            bool try_default = true; 

            for (const dd_ordering& ordering: it->second) {
                dd.load_data(dd_filename); 
                dd_ordering default_ordering("default", dd.v_order->var_order);

                if (try_default) {
                    defaultorderstats.push_back(
                        extract_ddstats(dd_name, default_ordering, dd)); 
                    try_default = false; 
                }
                // int last_variable = dd.v_order->var_order.at(dd.v_order->size()); //get node_id variable 

                dd.set_variable_ordering(ordering.ordering); 
                chosenorderstats.push_back(
                    extract_ddstats(dd_name, ordering, dd)); 

                // std::reverse(reverse_orderev_ordering.begin() + 1, rev_ordering.end()); 
                dd_ordering reverse_ordering(ordering.name + "_reverse", ordering.ordering);
                mtmdd::var_order_t v = reverse_ordering.ordering;
                std::reverse(v.begin() + 1, v.end()); 
                dd.set_variable_ordering(v); 
                reverseorderstats.push_back(
                    extract_ddstats(dd_name, reverse_ordering, dd)); 
            }

/*
            const mtmdd::var_order_t& ordering = it->second;   
            mtmdd::var_order_t rev_ordering(ordering);

            dd.load_data(dd_filename); 
            defaultorderstats.push_back(extract_ddstats(dd_name, dd));
            int last_variable = dd.v_order->var_order.at(dd.v_order->size()); //get node_id variable

            dd.set_variable_ordering(ordering); 
            chosenorderstats.push_back(extract_ddstats(dd_name, dd));

            std::reverse(rev_ordering.begin() + 1, rev_ordering.end());
            dd.set_variable_ordering(rev_ordering); 
            reverseorderstats.push_back(extract_ddstats(dd_name, dd));   */

/*
            if (last_variable == ordering.back()) {
                mtmdd::var_order_t almost_reverse(ordering); 
                std::reverse(almost_reverse.begin() + 1, almost_reverse.end() - 1);
                dd.set_variable_ordering(almost_reverse); 
                almostreverseorderstats.push_back(extract_ddstats(dd_name, dd));
            } */
        }
    }
}


void ddtester::save_report(const std::string& outfolder, const std::string& outfilename) {
    std::ofstream fo(outfolder + "/" + outfilename, std::ios::out); 

    save_header(fo); 

    // ------------------ default ordering stats --------------------------
    for (const ddstats& x: defaultorderstats) 
        save_record(fo, x, x.ordering.name);

    // ------------------ chosen ordering stats --------------------------
    for (const ddstats& x: chosenorderstats)
        save_record(fo, x, x.ordering.name); 

    // ------------------ reverse chosen ordering stats --------------------------
    for (const ddstats& x: reverseorderstats)
        save_record(fo, x, x.ordering.name); 
    
    // for (const ddstats& x: almostreverseorderstats)
    //     save_record(fo, x, "rev_keep_id"); 
    
    
}
