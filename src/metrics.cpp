#include "heuristics.hpp"

/*
class fucking_metrics : public heuristic {
public: 
    fucking_metrics(mtmdd::MultiterminalDecisionDiagram& dd) : entropy_heuristic(dd) {
        for (auto x: unbounded_vars)
            std::cout << x << " ";
        std::cout << "\n";
    }
}; */

float calculate_metric(const std::map<combinations::combination, float>& hmap, std::vector<int> ordering);

void log2file(std::ofstream& fo, const std::map<combinations::combination, float>& hmap, const std::string& heuname);

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

    /**************************************************
     *  obtaining input parameter
     ****************************************************/
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

    /******************************************************
     * indexing graphs or loading index from disk 
     * ****************************************************/
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

    entropy_heuristic he(index_dd);
    // correlation_heuristic hc(index_dd);  
    std::cout << "entropy:\n";
    std::map<combinations::combination, float> m;
    he.metric_plot(m);


    std::ofstream fo(graph_file + ".metrics"); 

    log2file(fo, m, "entropy");

    correlation_heuristic hc(index_dd);
    hc.metric_plot(m);
    log2file(fo, m, "correlation"); 

    return 0; 
}


float calculate_metric(const std::map<combinations::combination, float>& hmap, std::vector<int> ordering) {
    float m = 0; 

    for (int i = 0; i < ordering.size(); ++i) {
        combinations::combination c(ordering.begin(), ordering.begin() + i + 1);
        auto it = hmap.find(c); 

        if (it != hmap.end()) 
            m += it->second;
    }

    return m; 
}

void log2file(std::ofstream& fo, const std::map<combinations::combination, float>& hmap, const std::string& heuname) {
    for (const auto& p: hmap) {
        const combinations::combination& seq = p.first;
        std::stringstream sseq;
        combinations::combination::iterator it = seq.begin(); 

        sseq << *it;
        for (++it; it != seq.end(); ++it)  sseq << "," << *it; 

        fo << sseq.str() << "\t" << p.second << "\t" << heuname << "\n";
    }
}