#include "heuristics.hpp"

inline void show_vec(mtmdd::var_order_t& v) {
    for (const auto& x: v)
        std::cout << x << " ";
    std::cout << "\n"; 
}

int main(int argc, char** argv) {
    MEDDLY::initialize(MEDDLY::defaultInitializerList(NULL)); 

    cxxopts::Options options(argv[0], "GRAPES-DD"); 
    options.add_options()
        ("i, in", "graph database filename", cxxopts::value<std::string>())
        ("l, lp", "max pathlength", cxxopts::value<int>()->default_value("4"))
        ("d, direct", "are graph direct?", cxxopts::value<std::string>()->default_value("true"));

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



    mtmdd::StatsDD stats; 
    mtmdd::var_order_t ordering; 
    index_dd.get_variable_ordering(ordering); 
    index_dd.get_stats(stats); 

    // std::cout << "Starting variable ordering: "; 
    // show_vec(ordering); 
    // stats.show(); 

    // heuristic *entropy = nullptr, *correlation = nullptr; 
    entropy_heuristic entropy(index_dd);
    correlation_heuristic correlation(index_dd);  


    std::cout << "entropy\n";
    ordering = entropy.get();
    show_vec(ordering);

    std::cout << "correlation\n";
    ordering = correlation.get(); 

    show_vec(ordering);

/*
    dd_heuristic y(index_dd); 
    y.correlation_heuristic();  */

/*
    if (false) {    
        //basic
        dd_heuristic x(index_dd);
        x.entropy_heuristic(ordering, false, false);
        std::cout << "Entropy ordering (ignoring terminals): "; 
        show_vec(ordering); 
        index_dd.set_variable_ordering(ordering); 
        index_dd.get_stats(stats); 
        stats.show(); 
        getchar();


        dd_heuristic y(index_dd); 
        y.correlation_heuristic(); 

        //count occurrences - use terminal as additional variable 
        dd_heuristic y(index_dd);
        y.entropy_heuristic(ordering, true, false);
        std::cout << "Entropy ordering (using terminals): "; 
        show_vec(ordering); 
        index_dd.set_variable_ordering(ordering); 
        index_dd.get_stats(stats); 
        stats.show();  
        getchar();


        //use terminal as variable + use variable also as counts 
        dd_heuristic w(index_dd);
        w.entropy_heuristic(ordering, true, true);
        std::cout << "Entropy ordering (using terminals): "; 
        show_vec(ordering); 
        index_dd.set_variable_ordering(ordering); 
        index_dd.get_stats(stats); 
        stats.show();  
        getchar();


        //use terminals as counts (x Marco)
        dd_heuristic z(index_dd);
        z.entropy_heuristic(ordering, false, true);
        std::cout << "Entropy ordering (using terminals): "; 
        show_vec(ordering); 
        index_dd.set_variable_ordering(ordering); 
        index_dd.get_stats(stats); 
        stats.show();
    } */
}