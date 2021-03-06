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

#ifndef MTDDS_HPP
#define MTDDS_HPP 

#include <unordered_map>
#include <meddly.h>
#include <meddly_expert.h>

#include "matching.hpp"
#include "buffer.hpp"
#include "dd_utils.hpp"

#include "OCPTreeListeners.h"
#include "GRAPESIndex.h"

#define NOP_LABEL 0 

class GraphsDB;


class GraphsDB {
public:
    std::queue<GRAPESLib::Graph> graphs_queue;
    GRAPESLib::LabelMap labelMap;
    unsigned total_num_vertices;  

    GraphsDB(const std::string& input_network_file, bool direct); 
};



namespace mtmdd {
    class MultiterminalDecisionDiagram; 
    class MtmddLoaderListener; //to load data from partial tries to the mtmdd
    class QueryListener; 



    inline void visit_edge(MEDDLY::dd_edge& edge) {
        size_t nlevels = edge.getForest()->getDomain()->getNumVariables(); 
        int value; 
        for (MEDDLY::enumerator e(edge); e; ++e) {
            const int *p = e.getAssignments(); 
            e.getValue(value); 
            for (int i = 1; i <= nlevels; ++i)
                std::cout << p[i] << " "; 
            std::cout << " ==> " << value << std::endl; 
        }
    }

    class MultiterminalDecisionDiagram {
    public:
        Encoder labelMapping; 
        GraphNodeEncoder graphNodeMapping; 

        bool direct_indexing = true; 
   //     size_t num_graphs_in_db = 0; 
    public: 
        VariableOrdering *v_order = nullptr;

        MEDDLY::forest::policies policy; 

        MEDDLY::forest *forest = nullptr; 
        MEDDLY::dd_edge *root = nullptr; 
        
        inline size_t num_indexed_graphs() const {
            // return num_graphs_in_db; 
            return graphNodeMapping.num_graphs();
        }
    private:
        void load_from_graph_db(const GraphsDB& graphs_db);

    public: 
        //empty decision diagram with uninitialized domain (has to be defined before initialization)
        MultiterminalDecisionDiagram() : policy(false) {
            policy.setPessimistic(); 
            // policy.setVarSwap();
            // policy.setRandom();  //
            // policy.setLowestCost();//uguale
        }

        /* build mtmdd from a network, by extracting all paths up to length = max_depth */ 
        MultiterminalDecisionDiagram(const std::string& input_network_file, unsigned max_depth, bool direct, size_t buffersize) 
        : MultiterminalDecisionDiagram() {
            GraphsDB graphs_db(input_network_file, direct);
            init(graphs_db, max_depth);
        }

        MultiterminalDecisionDiagram(const GraphsDB& graphs_db, unsigned max_depth)
        : MultiterminalDecisionDiagram() {
            init(graphs_db, max_depth/*, var_order*/); 
        }

        MultiterminalDecisionDiagram(const domain_bounds_t& bounds)
        : MultiterminalDecisionDiagram() {
            init(bounds); 
        }

        ~MultiterminalDecisionDiagram() {
            delete root; 
            MEDDLY::destroyForest(forest); 
            delete v_order; 
        }


        void init(const VariableOrdering& ordering) {
            //init mtmdd with default variable ordering, then set the chosen ordering 
            init(ordering.bounds); 
            set_variable_ordering(ordering.var_order);
        }  

        void init(const domain_bounds_t& bounds)  {
            //init domain 
            v_order = new VariableOrdering(bounds); 
            /* initialize a meddly forest having both internal and terminal nodes 
             * of integer type*/
            forest = v_order->domain->createForest(
                false, 
                MEDDLY::forest::INTEGER,           //internal nodes
                MEDDLY::forest::MULTI_TERMINAL,    //terminal nodes 
                policy
            );
            root = new MEDDLY::dd_edge(forest); 
            v_order->set_forest(forest); 
        }

        void init(const GraphsDB& graphs_db, const unsigned max_depth) {
            //init variables' domain
            domain_bounds_t bounds(max_depth + 1, graphs_db.labelMap.size() + 1); 
            bounds.back() = graphs_db.total_num_vertices + 1; 
            //init mtmdd data structure
            init(bounds); 
            //load labelled paths into mtmdd
            load_from_graph_db(graphs_db); 
        }

        /** Variable ordering methods **/ 
        inline void get_variable_ordering(var_order_t& var_order) {
            v_order->get(var_order); 
        }

        inline void set_variable_ordering(const var_order_t& var_order) {
            v_order->set(var_order); 
        }

        //it returns the mtmdd's number of levels 
        inline size_t size() const {
            return v_order->size();
        }

        //it stores the values contained in the Buffer structure into the current mtmdd 
        inline void insert(Buffer& buffer) {
            try {
                MEDDLY::dd_edge tmp(forest); 
                forest->createEdge(buffer.data(), buffer.values_data(), buffer.num_elements(), tmp); 
                MEDDLY::apply(MEDDLY::PLUS, *root, tmp, *root); 
                tmp.clear(); 
            } catch (MEDDLY::error& e) {
                std::cerr 
                    << "Data insertion into mtmdd failed with the following meddly error: " 
                    << e.getName() << " (code " << e.getCode() <<  ")" 
                    << " at line " << e.getLine() << std::endl; 
                throw MEDDLY::error(e); 
            }
        }

        //search all the occurrences of the query subgraph in the indexed graphs 
        std::vector<GraphMatch> match(const std::string& query_graph_file, unsigned nthreads, std::vector<double>& times); 

        //it creates a pdf file representing the current mtdd - nb. it requires graphviz library! 
        void writePicture(const std::string& pdffile) const {
            try {
                root->writePicture(pdffile.c_str(), "pdf");
            } catch (MEDDLY::error& e) {
                std::cerr 
                    << "Cannot create pdf file " << pdffile 
                    << ".pdf due to MEDDLY error ==> " << e.getName() << std::endl; 
            }
        }

        //it saves on a file the content of this mtmdd
        void write(const std::string& out_ddfile); 

        //it fill this mtmdd loading data from a file 
        void read(const std::string& in_ddfile, const size_t lp); 

        //it returns a set of stats of the current mtmdd 
        void get_stats(StatsDD& stats) const;

        void save_data(const std::string& filename); 
        void load_data(const std::string& filename);
    }; 

    class QueryListener : public GRAPESLib::OCPTreeVisitListener {
    public: 
        Buffer buffer; 
        QueryPattern query; 
        const VariableOrdering& ordering; 

        QueryListener(const VariableOrdering& var_ordering, size_t elem_size, bool set_values) 
        : ordering(var_ordering), buffer(elem_size, set_values), GRAPESLib::OCPTreeVisitListener() {
        }

        ~QueryListener() {}

        virtual void visit_node(GRAPESLib::OCPTreeNode& n); 
        
        virtual void visit_leaf_node(GRAPESLib::OCPTreeNode& n) { 
            (void) n; 
        }
    }; 


    class MtmddLoaderListener : public GRAPESLib::OCPTreeVisitListener {
    public: 
        Buffer& _buffer; 
        MultiterminalDecisionDiagram& _mtmdd; 

        MtmddLoaderListener(MultiterminalDecisionDiagram& mtmdd, Buffer& buffer)
        : _buffer(buffer), _mtmdd(mtmdd) {
        }

        virtual void visit_node(GRAPESLib::OCPTreeNode& n); 

        virtual void visit_leaf_node(GRAPESLib::OCPTreeNode& n) {
            (void) n;
        }

    };
}

namespace grapes2dd {
    inline std::string get_dd_index_name(const std::string& input_network_file, const size_t lp) {
        return input_network_file + "." + std::to_string(lp) + ".index.mtdd";
    }

    inline bool dd_already_indexed(const std::string& input_network_file, const size_t lp) {
        return std::ifstream(get_dd_index_name(input_network_file, lp)).good(); 
    }

    inline size_t get_path_from_node(GRAPESLib::OCPTreeNode& n, std::vector<node_label_t>& vpath, const size_t max_pathlength = 0) {
        //retrieve labels from GRAPES node
        vpath.push_back(n.label + 1); 

        for (GRAPESLib::OCPTreeNode* p = n.parent; p; p = p->parent) {
            if (p->parent) {
                vpath.push_back(p->label + 1); 
            }
        }
        //fill vector with empty labels if maxlength is set, then return the current path length
        size_t pathlength = vpath.size(); 
        while (vpath.size() < max_pathlength) {
            vpath.insert(vpath.begin(), NOP_LABEL);  
        }

        return pathlength; 
    }
}

#endif