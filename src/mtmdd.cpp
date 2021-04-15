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

#include <queue> 
#include <chrono>

#include "mtmdd.hpp"

#include "Graph.h"
#include "LabelMap.h"
#include "GraphReaders.h"
#include "sbitset.h"
#include "OCPTreeNode.h"
#include "OCPTree.h"
#include "BuildManager.h"
#include "GraphPathListener.h"
#include "GraphVisit.h"
#include "BuildRunner.h"



using namespace mtmdd; 




GraphsDB::GraphsDB(const std::string& input_network_file, bool direct) : total_num_vertices(0) {
    std::ifstream is(input_network_file.c_str(), std::ios::in); 
    GRAPESLib::GraphReader_gff temp_greader(labelMap, is); 
    temp_greader.direct = direct; 
    bool keep_reading = true; 

    //read graph database 
    while (keep_reading) {
        GRAPESLib::Graph g(graphs_queue.size()); 

        if ((keep_reading = temp_greader.readGraph(g))) {
            total_num_vertices += g.nodes_count; 
            graphs_queue.push(g); 
        }
    }
}


MultiterminalDecisionDiagram::MultiterminalDecisionDiagram(const domain_bounds_t& bounds) 
: MultiterminalDecisionDiagram() {
    init(bounds); 
} 


void MultiterminalDecisionDiagram::load_from_graph_db(const GraphsDB& graphs_db) {
    std::queue<GRAPESLib::Graph> graphs_queue(graphs_db.graphs_queue);
    GRAPESLib::LabelMap labelMap(graphs_db.labelMap);
    int total_num_vertices = graphs_db.total_num_vertices;
    unsigned num_vars = v_order->domain()->getNumVariables();
    unsigned max_depth = num_vars - 1;
    unsigned buffersize = 10000; 

    Buffer dd_buffer(buffersize, num_vars + 1, true); 
    MtmddLoaderListener mlistener(*this, dd_buffer); 

    while (!graphs_queue.empty()) {
        GRAPESLib::Graph& current_graph = graphs_queue.front(); 
        std::map<int, std::vector<GRAPESLib::GNode*>> nodes_per_label; 

        //group nodes of the current graph by their labels
        for (int i = 0; i < current_graph.nodes_count; ++i) {
            nodes_per_label.emplace(
                std::piecewise_construct, 
                std::forward_as_tuple(current_graph.nodes[i].label), 
                std::forward_as_tuple()
            ).first->second.push_back(&current_graph.nodes[i]) ; 
        }

        for (auto& entryMap: nodes_per_label) {
            //starting DFS-Visit from nodes with the same label
            GRAPESLib::OCPTree partial_index; 
            GRAPESLib::AllPathListener plistener(&partial_index, current_graph.id); 
            GRAPESLib::DFSGraphVisitor gvisitor(plistener); 

            for (auto it = entryMap.second.begin(); it != entryMap.second.end(); ++it) {
                gvisitor.run(current_graph, *(*it), max_depth); 
            }

            //transfer data from trie to mtmdd via buffer
            partial_index.visit(mlistener); 
        }

        graphs_queue.pop(); 
    }

    //flush the remaining data from buffer to mtmdd
    if (dd_buffer.num_elements() > 0) {
        insert(dd_buffer); 
    }

    labelMapping.initFromGrapesLabelMap(labelMap); 
    graphNodeMapping.build_inverse_mapping();    
    num_graphs_in_db = graphNodeMapping.num_graphs();   
}

MultiterminalDecisionDiagram::MultiterminalDecisionDiagram(const std::string& input_network_file, unsigned max_depth, bool direct, size_t buffersize) 
: MultiterminalDecisionDiagram() {
    GraphsDB graphs_db(input_network_file, direct);
    init(graphs_db, max_depth);
}


MultiterminalDecisionDiagram::MultiterminalDecisionDiagram(const GraphsDB& graphs_db, unsigned max_depth, var_order_t& var_order)
: MultiterminalDecisionDiagram() {
    init(graphs_db, max_depth, var_order); 
}   



void MultiterminalDecisionDiagram::init(const domain_bounds_t& bounds, const var_order_t& var_order) {
    v_order = var_order.empty() 
              ? new VariableOrdering(bounds)             //default order 
              : new VariableOrdering(bounds, var_order); //user-defined order 
    forest = this->v_order->domain()->createForest(
        false, 
        MEDDLY::forest::INTEGER,                        //internal nodes have integer values 
        MEDDLY::forest::MULTI_TERMINAL,                 //and also terminal nodes 
        policy
    );
    root = new MEDDLY::dd_edge(forest); 
}

void MultiterminalDecisionDiagram::init(const GraphsDB& graphs_db, const unsigned max_depth, const var_order_t& var_order) {
    //init variables' domain
    domain_bounds_t bounds(max_depth + 1, graphs_db.labelMap.size() + 1); 
    bounds.back() = graphs_db.total_num_vertices + 1; 
    //init mtmdd data structure
    init(bounds, var_order); 
    //load labelled paths into mtmdd
    load_from_graph_db(graphs_db); 
}

void MultiterminalDecisionDiagram::write(const std::string& out_ddfile) {
    std::string outfilename = grapes2dd::get_dd_index_name(out_ddfile, size() - 1);
    std::ofstream fo(outfilename); 

    fo  << size() << " "                    //mtmdd depth 
        << labelMapping.size() << " "       //number of labels
        << graphNodeMapping.size() << "\n"  //number of vertices 
        << *v_order                         //variable ordering
        << labelMapping                     //labels sorted by mapped value 
        << graphNodeMapping;                //graphs nodes sorted by mapped values 
    fo.close(); 

    FILE *fp = fopen(outfilename.c_str(), "a"); 
    MEDDLY::FILE_output handler(fp); 
    forest->writeEdges(handler, this->root, 1);
    fclose(fp); 
}


void MultiterminalDecisionDiagram::read(const std::string& in_ddfile, const size_t lp) {
    std::string infilename = grapes2dd::get_dd_index_name(in_ddfile, lp); 
    std::ifstream fi(infilename, std::ios::in);
    int depth, nlabels, nvertices; 

    fi >> depth >> nlabels >> nvertices; 
    VariableOrdering v_order(depth); 
    v_order.read(fi); 
    init(v_order);

    labelMapping.read(fi, nlabels);   
    graphNodeMapping.read(fi);  
    num_graphs_in_db = graphNodeMapping.num_graphs();
    
    fi.close();

    FILE *fp = fopen(infilename.c_str(), "r");
    /** skip those lines containing metadata we've already read; 
     *  specifically: one line for each:
     *  - mtmdd level, 
     *  - label,
     *  - graph, 
     *  PLUS two additional lines (the header and the total number of graphs) */
    for (int lines_to_skip = depth + nlabels + num_graphs_in_db + 2; lines_to_skip > 0; --lines_to_skip) 
        if (fscanf(fp, "%*[^\n]\n") != 0)
            throw std::logic_error("This cannot happen! If it happens, there is something really wrong!");

    MEDDLY::FILE_input handler(fp);
    forest->readEdges(handler, this->root, 1);
    fclose(fp);  
}

void MultiterminalDecisionDiagram::get_stats(StatsDD& stats) const {
    stats.num_nodes = forest->getCurrentNumNodes(); 
    stats.peak_nodes = forest->getPeakNumNodes(); 
    stats.memory_used = forest->getCurrentMemoryUsed();
    stats.peak_memory = forest->getPeakMemoryUsed(); 
    stats.num_unique_nodes = root->getNodeCount();
    stats.num_edges = root->getEdgeCount(); 
    stats.num_graphs = num_graphs_in_db; 
    stats.num_labels = labelMapping.size();
    stats.num_vars = v_order->domain()->getNumVariables(); 
    //domain->getNumVariables();
    MEDDLY::apply(MEDDLY::CARDINALITY, *root, stats.cardinality);          
}


void MtmddLoaderListener::visit_node(GRAPESLib::OCPTreeNode& n) {
    const size_t max_pathlength = _buffer.element_size - 2; 
    std::vector<node_label_t> path; 
    size_t pathlength = grapes2dd::get_path_from_node(n, path, max_pathlength); 

    for (auto oit = n.gsinfos.begin(); oit != n.gsinfos.end(); ++oit) {
        //iterate over starting nodes of the current path in the current graph 
        for (sbitset::iterator sit = oit->second.from_nodes.first_ones(); sit != oit->second.from_nodes.end(); sit.next_ones()) { 
            //get the first slot in the buffer 
            Buffer::buffer_slot_t buffer_slot(_buffer.get_slot()); 
      
            //store labels and starting node in the buffer slot 
            _mtmdd.v_order->copy_variables(
                path,                                                //labelled path of length L 
                _mtmdd.graphNodeMapping.map(oit->first, sit.first),  //starting vertex of the path (encoded)
                1 + buffer_slot.first                                //destination (array of length L + 1)
            );  
            //store number of occurrences of the path in the current graph 
            _buffer.save_value(oit->second.path_occurrence); 

            if (buffer_slot.second == false) {
                _mtmdd.insert(_buffer); 
                _buffer.flush(); 
            }
        }
    }
}

void QueryListener::visit_node(GRAPESLib::OCPTreeNode& n) {
    LabelledPath labelled_path; 
    const size_t max_pathlength = buffer.element_size - 2; 
    size_t pathlength = grapes2dd::get_path_from_node(n, labelled_path, max_pathlength);

    //iterate over paths 
    for (auto oit = n.gsinfos.begin(); oit != n.gsinfos.end(); ++oit) {
        /* Since (1) we do not save the starting node id in the DD and 
         * (2) the occurrence number is the total occurrence number of that path in the query graph, 
         * each path is saved once in the buffer. Otherwise, during DD construction, the occurrence 
         * numbers are summed up and the resulting DD is a complete mess! */ 
        bool insert_in_buffer_flag = true; 

        //iterate over starting nodes of the current path in the current graph 
        for (sbitset::iterator sit = oit->second.from_nodes.first_ones(); sit != oit->second.from_nodes.end(); sit.next_ones()) {
            if (insert_in_buffer_flag) {
                //store labels  
                Buffer::buffer_slot_t buffer_slot(buffer.push_slot(oit->second.path_occurrence)); 

                //copy labelled path in the buffer slot, replacing starting node information with a placeholder  
                ordering.copy_variables(labelled_path, MEDDLY::DONT_CARE, buffer_slot.first + 1);

                //associate buffer location to the labelled path 
                labelled_path.assign_pointer2buffer(buffer_slot.first); 
                labelled_path.set_occurrence_number(oit->second.path_occurrence); 
                insert_in_buffer_flag = false; 
            }

            //build query object 
            query.add_path_to_node(sit.first, labelled_path);
        }
    }
}


std::vector<GraphMatch> MultiterminalDecisionDiagram::match(const std::string& query_graph_file, unsigned nthreads, std::vector<double>& times) {
    std::ifstream is(query_graph_file.c_str(), std::ios::in);
    GRAPESLib::OCPTree query_tree;
    const int max_depth = size() - 1;
    const VariableOrdering& var_ordering = *v_order; 

    time_point start_query_indexing, start_dd_intersection, start_query_filtering; 
    time_point end_query_indexing, end_dd_intersection, end_query_filtering; 

    // start query indexing 
    start_query_indexing = std::chrono::_V2::steady_clock::now(); 

    GRAPESLib::LabelMap grapesLabelMap; 
    labelMapping.initGrapesLabelMap(grapesLabelMap); 

    //1. create query trie 
    GRAPESLib::GraphReader_gff greader(grapesLabelMap, is);
    greader.direct = direct_indexing;
    GRAPESLib::DefaultBuildManager bman(query_tree, greader, max_depth, nthreads);
    GRAPESLib::OnePathListener plistener;
    GRAPESLib::DFSGraphVisitor gvisitor(plistener);
    GRAPESLib::BuildRunner buildrun(bman, gvisitor, 1);
    buildrun.run();
    is.close();

    //2. create dd without no node info from trie 
    QueryListener ql(var_ordering, max_depth + 2, true); 
    query_tree.visit(ql); 

    end_query_indexing = std::chrono::_V2::steady_clock::now();
    times.push_back(get_time_interval(end_query_indexing, start_query_indexing)); 
    //end query indexing

    Buffer& b = ql.buffer; 
    //extract query paths from index
    start_dd_intersection = std::chrono::_V2::steady_clock::now(); 

    MEDDLY::dd_edge query_dd(forest), query_matched(forest); 
    forest->createEdge(b.data(), b.values_data(), b.num_elements(), query_dd); 
    MEDDLY::apply(MEDDLY::MULTIPLY, *root, query_dd, query_matched); 

    end_dd_intersection = std::chrono::_V2::steady_clock::now();
    times.push_back(get_time_interval(end_dd_intersection, start_dd_intersection)); 
    //end extraction paths from dd 

    //start filtering query results 
    std::vector<GraphMatch> matched_graphs; 
    start_query_filtering = std::chrono::_V2::steady_clock::now(); 

    QueryPattern& qpattern = ql.query; 
    qpattern.assign_dd_edge(&query_dd); 
    MatchedQuery mq(qpattern, graphNodeMapping, var_ordering); 
    mq.match(query_matched, matched_graphs); 

    end_query_filtering = std::chrono::_V2::steady_clock::now();
    times.push_back(get_time_interval(end_query_filtering, start_query_filtering)); 

    //remove temporary dd nodes 
    query_dd.clear(); 
    query_matched.clear(); 

    return matched_graphs; 
}


void MultiterminalDecisionDiagram::save_data(const std::string& filename) {
    std::ofstream fout(filename, std::ios::out); 
    const var_order_t& order = v_order->var_order();
    const domain_bounds_t& domains = v_order->bounds(); 
    const int offset = order.size() + 1; 

    for (auto it = order.begin(); it != order.end(); ++it)  fout << "v_" << *it << "\t";
    fout << "terminal\n";

    for (auto it = domains.begin(); it != domains.end(); ++it)  fout << *it << "\t";
    fout << "\n"; 
    
    for (MEDDLY::enumerator e(*root); e; ++e) {
        const int *variables = e.getAssignments(); 
        int value; 
        e.getValue(value); 

        std::ostringstream stream; 
        std::copy(variables + 1, variables + offset, std::ostream_iterator<int>(stream, "\t"));
        fout << stream.str() << value << "\n";
    }
}


void MultiterminalDecisionDiagram::load_data(const std::string& filename) {
    std::ifstream fin(filename, std::ios::in); 
    var_order_t order; 
    domain_bounds_t bounds; 
    Parser parser("\t");
    std::string line, line1; 

    //first two lines are for variable order and variable's domain
    std::getline(fin, line); 
    std::getline(fin, line1); 

    //parse variables 
    parser.set_string(line.c_str()); 
    try {
        char *p; 
        while (p = parser.parse()) 
            order.push_back(std::stoi(std::string(p+2)));
    } catch (std::exception& e) {}

    //parse domains
    parser.set_string(line1.c_str());
    try {
        while (true) 
            bounds.push_back(parser.parseint()); 
    } catch (std::exception& e) {}

    //initialize empty mtmdd 
    init(VariableOrdering(bounds, order, false));

    Buffer buffer(50000, order.size() + 1);
    
    //fill data into mtmdd 
    try {
        while (fin.good()) {
            Buffer::buffer_slot_t slot(buffer.get_slot());
            std::getline(fin, line); 
            parser.set_string(line.c_str()); 
            
            for (int i = 1; i <= bounds.size(); ++i)
                slot.first[i] = parser.parseint(); 

            buffer.save_value(parser.parseint());

            if (!slot.second) {
                insert(buffer); 
                buffer.flush(); 
            }
        }
    } catch (std::invalid_argument& e) {
    }

    if (buffer.num_elements() > 0) {
        insert(buffer); 
    }
}