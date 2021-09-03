#include "heuristics.hpp"

inline void show_vec(mtmdd::var_order_t& v) {
    for (const auto& x: v)
        std::cout << x << " ";
    std::cout << "\n"; 
}

inline std::string str_vec(mtmdd::var_order_t& v) {
    std::stringstream ss; 
    ss << v.at(0); 
    for (mtmdd::var_order_t::iterator it = v.begin() + 1; it != v.end(); ++it) 
        ss << "," << *it; 
    return ss.str(); 
}

int main(int argc, char** argv) {
    MEDDLY::initialize(MEDDLY::defaultInitializerList(NULL)); 

    cxxopts::Options options(argv[0], "GRAPES-DD"); 
    options.add_options()
        ("i, in", "graph database filename", cxxopts::value<std::string>())
        ("l, lp", "max pathlength", cxxopts::value<int>()->default_value("4"))
        ("d, direct", "are graph direct?", cxxopts::value<std::string>()->default_value("true"));
        // ("o, out", "output filename", cxxopts::value<std::string>());

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


    //if heuristic output file exists, then test 
    std::string dbname(graph_file.substr(graph_file.find_last_of("/") + 1, graph_file.length()));
    std::ifstream fi(output_folder + "/" + dbname + ".heu");
    std::map<std::string, mtmdd::var_order_t> ordermap; 

    if (fi.good()) {
        Parser parser(",");

        for (int i = 0; fi.good(); ++i) { //read file line by line 
            std::string hname, hord; 
            fi >> hord >> hname; 

            if (i) { //skipping header 
                parser.set_string(hord.c_str()); //parse ordering string 
                mtmdd::var_order_t vec; 
                
                try { //save ordering in a vector 
                    while (true) { 
                        vec.push_back(parser.parseint());
                    }
                } 
                catch (std::invalid_argument& e) {
                    if (vec.size())
                        ordermap.emplace(
                            std::piecewise_construct, 
                            std::forward_as_tuple(hname),
                            std::forward_as_tuple(vec)
                        );
                }
            }
        }

        std::ofstream fo(output_folder + "/" + dbname + ".heu.stats"); 
        mtmdd::StatsDD stats; 

        //put baseline (default ordering) in map 
        index_dd.get_stats(stats);
        ordermap.emplace(std::piecewise_construct, std::forward_as_tuple("default"), std::forward_as_tuple(stats.ordering));

        fo << "method\tnum_nodes\tnum_edges\tmemory_used\tordering\n";

        for (const auto& pair: ordermap) { //test orderings over DD 
            index_dd.set_variable_ordering(pair.second); 
            index_dd.get_stats(stats);

            fo 
                << pair.first << "\t" 
                << stats.num_unique_nodes << "\t"
                << stats.num_edges << "\t"
                << stats.memory_used << "\t"
                << str_vec(stats.ordering) << "\n";
        }

        return 0; 
    }

    

/*
    mtmdd::StatsDD stats; 
    mtmdd::var_order_t ordering; 
    index_dd.get_variable_ordering(ordering); 
    index_dd.get_stats(stats); 

    stats.show();  */

    // std::cout << "Starting variable ordering: "; 
    // show_vec(ordering); 
    // stats.show(); 

    // heuristic *entropy = nullptr, *correlation = nullptr; 
    entropy_heuristic entropy(index_dd);
    correlation_heuristic correlation(index_dd);  

    std::cout << "Calculating entropy heuristic..." << std::endl; 
    ordermap.emplace(
        std::piecewise_construct, 
        std::forward_as_tuple("entropy"), 
        std::forward_as_tuple(entropy.get())
    );

    std::cout << "Calculating correlation heuristic..." << std::endl; 
    ordermap.emplace(
        std::piecewise_construct, 
        std::forward_as_tuple("correlation"), 
        std::forward_as_tuple(correlation.get())
    ); 


    {
        std::ofstream fo(output_folder + "/" + dbname + ".heu");
        fo << "ordering\theuristic\n"; 

        for (const auto& mapEntry: ordermap) {
            std::string hname; 
            mtmdd::var_order_t ordering; 

            std::tie(hname, ordering) = mapEntry;
            ordering.insert(ordering.begin(), 0); 
            ordering.push_back(ordering.size()); 

            //save current ordering 
            fo << str_vec(ordering) << "\t" << hname << "\n"; 

            //... and its reverse 
            std::reverse(ordering.begin() + 1, ordering.end() - 1); 
            fo << str_vec(ordering) << "\t" << hname << "_rev\n";
        }
    }



/*
    // std::cout << "work by entropy:\n"; 
    mtmdd::var_order_t entropy_order(entropy.get()); 




    mtmdd::var_order_t correlation_order(correlation.get()); 




    const mtmdd::var_order_t& entropy_order = entropy.get();
    mtmdd::var_order_t tmp(entropy_order); 
    tmp.insert(tmp.begin(), 0); 
    tmp.push_back(tmp.size()); 

    std::cout << "entropy v: "; 
    show_vec(tmp); 
    std::cout << "\nreverse? : "; 
    std::reverse(tmp.begin() + 1, tmp.end() - 1); 
    show_vec(tmp); 


    
    // show_vec(entropy_order);

    std::cout << "work by correlation:\n"; 
    const mtmdd::var_order_t& corre_order = correlation.get(); 
    std::cout << "order obtained: " <<   show_vec(corre_order) << "\n";  */
    // correlation.find(); 


    // std::cout << "best stuff:\n";
    // heuristic::cache_ordering.best_entry().show();
/*
    auto map = correlation.cache_ordering.get_cache();
    std::string dbname(graph_file.substr(graph_file.find_last_of("/") + 1, graph_file.length()));

    std::string outfilename(output_folder + "/" + dbname + ".heu");

    std::ofstream fo(outfilename, std::ios::out);

    std::vector<std::string> header{"n_dd_edges", "n_dd_nodes", "memory_used", "num_graphs", "num_labels", "num_elements", "v_ord"};
    fo << header[0];
    for (auto it = header.begin() + 1; it != header.end(); ++it)
        fo << "\t" << *it; 
    fo << "\n";

    for (auto const& x: map) {
        fo 
            << x.second.num_edges << "\t" 
            << x.second.num_nodes << "\t" 
            << x.second.memory_used << "\t" 
            << x.second.num_graphs << "\t" 
            << x.second.num_labels << "\t" 
            << x.second.cardinality << "\t"
            << x.first << "\n";
    } */
}