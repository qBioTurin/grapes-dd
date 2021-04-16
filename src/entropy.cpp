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

class MyListener; 

class MyListener: public GRAPESLib::GraphPathListener {
    class count_matrix {
    public: 
        std::vector<std::vector<unsigned>> matrix;

        count_matrix() {}

        count_matrix(unsigned nrows, unsigned ncols) : matrix(nrows) {
            for (auto it = matrix.begin(); it != matrix.end(); ++it)
                it->resize(ncols, 0); 
        }

        count_matrix(const count_matrix& cm) : matrix(cm.matrix.size()) {
            std::copy(cm.matrix.begin(), cm.matrix.end(), matrix.begin());
        }

        inline void resize(unsigned nrows, unsigned ncols) {
            matrix.resize(nrows); 
            for (auto it = matrix.begin(); it != matrix.end(); ++it)
                it->resize(ncols, 0); 
        }

        inline void increment(unsigned row, unsigned col) {
            ++matrix[row][col];
        }

        void show() {
            for (auto rit = matrix.begin(); rit != matrix.end(); ++rit) {
                for (auto cit = rit->begin(); cit != rit->end(); ++cit)
                    std::cout << *cit << " ";
                std::cout << std::endl; 
            }   
        }
    }; 

   // GRAPESLib::GNode* prev_node; 
   std::stack<GRAPESLib::GNode*> previous;

    unsigned curr_depth; 

public:

    /** #vertices x Lp matrix:
     * */ 
    count_matrix node_matrix; 
    /** #labels x Lp matrix:
     * */ 
    count_matrix label_matrix; 
    /** #labels x #labels matrix: 
     * */ 
    count_matrix labelxlabel_matrix; 

    std::vector<count_matrix> labelxlabelxdepth_matrix; 

	MyListener() : GraphPathListener() {}

    MyListener(GRAPESLib::Graph& g, unsigned max_depth, unsigned num_labels)
            : GraphPathListener(), 
              node_matrix(g.nodes_count, max_depth), 
              label_matrix(num_labels, max_depth), 
              labelxlabel_matrix(num_labels, num_labels) {

        for (int i = 1; i < max_depth; ++i)
            labelxlabelxdepth_matrix.emplace_back(num_labels, num_labels);
    }


    MyListener(const MyListener& l) 
            :   node_matrix(l.node_matrix), 
                label_matrix(l.label_matrix) {
		this->graph_id = l.graph_id;
		this->start_vertex_id = l.start_vertex_id;
	}

	virtual MyListener& clone(){
		return *(new MyListener(*this)); 
	}

	virtual void start_vertex(GRAPESLib::GNode& n){
		start_vertex_id = n.id;
        curr_depth = 0; 

        node_matrix.increment(n.id, 0);
        label_matrix.increment(n.label, 0);

        previous.push(&n); 
	}
    
	virtual void discover_vertex(GRAPESLib::GNode& n){
        GRAPESLib::GNode& prev_node = *(previous.top()); 
        ++curr_depth;

        labelxlabel_matrix.increment(prev_node.label, n.label);

        labelxlabelxdepth_matrix.at(curr_depth-1).increment(prev_node.label, n.label); 


        node_matrix.increment(n.id, curr_depth);
        label_matrix.increment(n.label, curr_depth);

        previous.push(&n); 
	}

	virtual void finish_vertex(GRAPESLib::GNode& n){
        --curr_depth;
        previous.pop(); 
	}
};


void heuristic(const GraphsDB& db, unsigned max_depth); 

void path_extraction(
        mtmdd::MultiterminalDecisionDiagram& dd, 
        const GraphsDB& db, 
        unsigned max_depth, 
        unsigned dfs_per_label, 
        const std::string& output_file);

void extract_labelled_paths(
        mtmdd::MultiterminalDecisionDiagram& dd, 
        const GraphsDB& db, 
        unsigned max_depth, 
        unsigned dfs_per_label); 

size_t get_filesize(const std::string& filename); 

std::string format(long a, long b); 

size_t build_dd(
        const std::string& db_filename, 
        const GraphsDB& db, 
        unsigned max_depth, 
        mtmdd::StatsDD& stats, 
        const mtmdd::var_order_t& variables = {}); 


int main(int argc, char** argv) {
    MEDDLY::initialize(MEDDLY::defaultInitializerList(NULL)); 

    cxxopts::Options options(argv[0], "GRAPES-DD"); 
    options.add_options()
        ("i, in", "graph database filename", cxxopts::value<std::string>())
        ("o, out", "out filename", cxxopts::value<std::string>())
        ("l, lp", "max pathlength", cxxopts::value<int>()->default_value("4"))
  //      ("d, direct", "are graph direct?", cxxopts::value<std::string>()->default_value("true")) 
  //      ("t, nthreads", "number of threads to use", cxxopts::value<int>()->default_value("8")) 
        ("b, bsize", "size buffer to load data into mtmdd", cxxopts::value<int>()->default_value("10000"))
        ("v, varorder", "mtmdd variable ordering", cxxopts::value<mtmdd::var_order_t>())
        ("dfs", "dfs visit per label per graph", cxxopts::value<int>()->default_value("5"))
        ;

    cxxopts::ParseResult result = options.parse(argc, argv); 
    std::string graph_file, query_file, output_folder, output_file; 
    int dfs_per_label, buffersize;
    unsigned max_depth; 
    bool direct_graph, build_full_dd = false;

    mtmdd::var_order_t variables;


    try {
        graph_file.assign(result["in"].as<std::string>()); 
//        output_folder.assign(graph_file.substr(0, graph_file.find_last_of("/"))); 
        output_file.assign(result["out"].as<std::string>());
        max_depth = result["lp"].as<int>();
        buffersize = result["bsize"].as<int>();
        dfs_per_label = result["dfs"].as<int>(); 

        std::string ext = ".gfd";
        direct_graph = graph_file.compare(graph_file.length() - ext.length(), ext.length(), ext) == 0; 

        if (result["varorder"].count() > 0) {
            variables = result["varorder"].as<mtmdd::var_order_t>();
            //todo- check correctness 
            build_full_dd = true;
        } else {
            //impose default order 
            variables.resize(max_depth + 1); 
            std::iota(variables.begin(), variables.end(), 0);
        }
    } catch (std::domain_error& ex) {
        std::cout << "Missing IO parameters!\n";
        std::cout << options.help() << std::endl; 
        return 1; 
    }

    //load graphs from file 
    GraphsDB db(graph_file, direct_graph);  

    if (build_full_dd) {
        std::cout << "Building mtmdd from graphs db...\n" 
                  << "Using the following variable ordering: "; 

        for (const auto& x: variables)  std::cout << x << " ";


        mtmdd::StatsDD stats_default, stats_order;

        std::cout << "\nBuilding DD with default variable ordering..." << std::endl; 
        size_t size_default = build_dd(graph_file, db, max_depth, stats_default);

        std::cout << "Building DD with custom variable ordering..." << std::endl; 
        size_t size_custom = build_dd(graph_file, db, max_depth, stats_order);  

        std::cout << 
            "Num nodes: " << format(stats_order.num_nodes, stats_default.num_nodes) <<
            "Peak nodes: " << format(stats_order.peak_nodes, stats_default.peak_nodes) <<
            "Num edges: " << format(stats_order.num_edges, stats_default.num_edges) <<
            "Memory used: " << format(stats_order.memory_used, stats_default.memory_used) <<
            "Peak memory: " << format(stats_order.peak_memory, stats_default.peak_memory) <<
            "Index size: " << format(size_custom, size_default) << 
            "Cardinality: " << format(stats_order.cardinality, stats_default.cardinality) << std::endl; 

    } else {
        heuristic(db, max_depth); 


        /** Path extraction **/
        // mtmdd::MultiterminalDecisionDiagram dd;
        // path_extraction(dd, db, max_depth, dfs_per_label, output_file); 
    }

    return 0; 
}


void path_extraction(
        mtmdd::MultiterminalDecisionDiagram& dd, 
        const GraphsDB& db, 
        unsigned max_depth, 
        unsigned dfs_per_label, 
        const std::string& output_file) {

    mtmdd::domain_bounds_t bounds(max_depth + 1, db.labelMap.size() + 1); 
    bounds.back() = MEDDLY::DONT_CARE; //unknown number of nodes 
    
    mtmdd::VariableOrdering var_order(bounds); 
    dd.init(var_order); 

    std::cout << "Extracting labelled paths from graphs...\n" << std::endl; 
    extract_labelled_paths(dd, db, max_depth, dfs_per_label); 

    std::cout << "Saving data in " << output_file << std::endl;
    dd.save_data(output_file); 
}


void heuristic(const GraphsDB& db, unsigned max_depth) {
    for (std::queue<GRAPESLib::Graph> queue = db.graphs_queue; !queue.empty(); queue.pop()) {
        GRAPESLib::Graph& g = queue.front(); 
        MyListener listener(g, max_depth, db.labelMap.size()); 
        GRAPESLib::DFSGraphVisitor(listener).run(g, max_depth); 

        std::cout << "### Graph " << g.id << "\n"; 


        std::cout << "label matrix:\n"; 
        listener.label_matrix.show(); 


        // std::cout << "label x label matrix:\n";
        // listener.labelxlabel_matrix.show(); 


        // for (int i = 0; i < listener.labelxlabelxdepth_matrix.size(); ++i) {
        //     std::cout << "At depth = " << i << std::endl; 
        //     listener.labelxlabelxdepth_matrix.at(i).show();
        // }
    }
}



void extract_labelled_paths(mtmdd::MultiterminalDecisionDiagram& dd, const GraphsDB& db, unsigned max_depth, unsigned dfs_per_label) {
    Buffer buffer(10000, dd.size() + 1, true); 
    mtmdd::MtmddLoaderListener mlistener(dd, buffer); 

    for (std::queue<GRAPESLib::Graph> queue = db.graphs_queue; !queue.empty(); queue.pop()) {
        GRAPESLib::Graph& current_graph = queue.front(); 

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
        for (int nodes_count = current_graph.nodes_count, i = 0; i < nodes_count; ) {
            const node_label_t curr_label = current_graph.nodes[i].label; 
            int dfs_counter = 0; 

            do {
                if (++dfs_counter <= dfs_per_label) {
                    gvisitor.run(current_graph, current_graph.nodes[i], max_depth); 
                }
            } while (++i < nodes_count && curr_label == current_graph.nodes[i].label);
        }

        partial_index.visit(mlistener); 
    }

    if (buffer.num_elements() > 0) {
        dd.insert(buffer); 
    }
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
        const std::string& db_filename, 
        const GraphsDB& db, 
        unsigned max_depth, 
        mtmdd::StatsDD& stats, 
        const mtmdd::var_order_t& variables) {

    mtmdd::MultiterminalDecisionDiagram dd;
    dd.init(db, max_depth, variables); 
    dd.write(db_filename); 

    std::string index_filename = grapes2dd::get_dd_index_name(db_filename, max_depth);
    size_t size_dd = get_filesize(index_filename); 

    dd.get_stats(stats); 

    return size_dd; 
}