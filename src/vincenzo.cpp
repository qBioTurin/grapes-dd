#include <algorithm>
#include <iostream>
#include <chrono>
#include <stack>

#include "mtmdd.hpp"
#include "cxxopts.hpp"

#include "size_t.h"
#include "typedefs.h"

#include "Graph.h"
#include "GraphVisit.h"
#include "GraphPathListener.h"


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

int main(int argc, char** argv) {
    cxxopts::Options options(argv[0], ""); 
    options.add_options()
        ("i, in", "graph database filename", cxxopts::value<std::string>())
        ("l, lp", "max pathlength", cxxopts::value<int>()->default_value("4"))
        ("d, direct", "are graph direct?", cxxopts::value<std::string>()->default_value("true")) 
        ("t, nthreads", "number of threads to use", cxxopts::value<int>()->default_value("8"));

    cxxopts::ParseResult result = options.parse(argc, argv); 
    std::string graph_file, query_file, output_folder, log_file; 
    unsigned max_depth, nthreads, buffersize;
    bool direct_graph; 


    try {
        graph_file.assign(result["in"].as<std::string>()); 
        output_folder.assign(graph_file.substr(0, graph_file.find_last_of("/"))); 
        max_depth = result["lp"].as<int>();
        nthreads = result["nthreads"].as<int>();
        direct_graph = result["direct"].as<std::string>().compare("true") == 0; 
    } catch (std::domain_error& ex) {
        std::cout << "Missing IO parameters!\n";
        std::cout << options.help() << std::endl; 
        return 1; 
    }

    GraphsDB db(graph_file, direct_graph);

    std::cout 
        << "Loaded db\nNum vertices: " << db.total_num_vertices << "\n"
        << "Num graphs: " << db.graphs_queue.size() << std::endl;

    

    while (!db.graphs_queue.empty()) {
        GRAPESLib::Graph& g = db.graphs_queue.front(); 
        MyListener listener(g, max_depth, db.labelMap.size());
        GRAPESLib::DFSGraphVisitor(listener).run(g, max_depth); 

        std::cout << "###Graph " << g.id << "\n";
        // std::cout << "Labels\n";
        // listener.label_matrix.show();

        // listener.labelxlabel_matrix.show(); 

        for (int i = 0; i < listener.labelxlabelxdepth_matrix.size(); ++i) {
            std::cout << "At depth = " << i << std::endl; 
            listener.labelxlabelxdepth_matrix.at(i).show();
        }

        // std::cout << "###############\n";

        db.graphs_queue.pop();
    }

    return 0;
}