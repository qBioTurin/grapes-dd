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

#ifndef UTILS_HPP
#define UTILS_HPP

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <exception>
#include <map>
#include <numeric>
#include <string>
#include <stdexcept>
#include <sstream>


#include <meddly.h>      
#include <meddly_expert.h>

#include "LabelMap.h"

#define DEBUG 0
#define WRITE_MDD_PDF 0


class Parser; 

using time_point = std::chrono::_V2::steady_clock::time_point; 


inline double get_time_interval(time_point end_t, time_point start_t) {
    return std::chrono::duration_cast<std::chrono::duration<double>>(end_t - start_t).count(); 
}


template<typename T> 
std::string show_vec(std::vector<T> v) {
    std::stringstream ss;
    ss << v[0];
    for (auto it = v.begin() + 1; it != v.end(); ++it)   
        ss << "," << *it;
    return ss.str();
}


class Parser {
    const size_t buffersize = 256; 

    std::string delimiters; 
    char *buffer; 
    bool state; 
public:
    inline Parser(const char* delimiters) : state(false) {
        buffer = new char[buffersize];
        set_delimiters(delimiters);
    }

    inline ~Parser() {
        delete buffer; 
    }

    inline void set_string(const char *str) {
        strncpy(this->buffer, str, buffersize);
        this->state = false; 
    }

    inline void set_delimiters(const char* delimiters) {
        this->delimiters.assign(delimiters); 
    }

    inline char* parse() {
        char *ret = state ? strtok(NULL, delimiters.c_str()) : strtok(buffer, delimiters.c_str()); 
        state = ret != NULL; 
        return ret; 
    }

    inline int parseint() {
        char* p = this->parse();

        if (!p) 
            throw std::invalid_argument("Invalid token"); 

        return atoi(p); 
    }
};


namespace mtmdd {
    class VariableOrdering; 
    struct StatsDD;
    class GraphNodeEncoder; 
    class Encoder;


    //it identifies one of the indexed graphs 
    using graph_id_t = unsigned int; 

    //it identifies one node of a specific graph
    using node_id_t = unsigned int; 

    //it represent a specific node of a specific graph 
    using graph_node_t = std::pair<graph_id_t, node_id_t>; 

    //vector of integers used to initialize the domain of mtmdd's levels
    using domain_bounds_t = std::vector<int>;

    using var_order_t = std::vector<int>; 

    //vector of integers used to define variable ordering to impose to MTMDD levels
    // using var_order_t = std::vector<int>; 
    using variable_vec_t = std::vector<MEDDLY::variable*>;

    //vector of strings used to perform path queries on MDDs 
    using label_path_t = std::vector<std::string>; 

    //(graph_id + node_id infos) + num_occurrences 
    using single_result_t = std::pair<graph_node_t, int>; 

    //vector of single query results 
    using query_result_t = std::vector<single_result_t>; 


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


    class VariableOrdering {
    public:
        // the i-th value is the domain of the i-th variable 
        domain_bounds_t bounds; 
        // the i-th value is the desired position of the i-th variable in the order 
        var_order_t var_order;  

        MEDDLY::domain *domain = nullptr; 
    private:
        MEDDLY::expert_forest *forest = nullptr; 
        MEDDLY::variable **vars = nullptr;

        inline void init_meddly_domain() {
            const int num_vars = bounds.size(), num_labels = bounds.front(), num_vertices = bounds.back(); 
            vars = (MEDDLY::variable**) malloc((1+num_vars) * sizeof(void*));
            
            vars[0] = nullptr; 
            vars[num_vars] = MEDDLY::createVariable(num_vertices, nullptr); 
            
            for (int i = num_vars - 1; i > 0; --i) 
                vars[i] = MEDDLY::createVariable(num_labels, nullptr); 

            domain = MEDDLY::createDomain(vars, num_vars);
            
            if (var_order.empty()) {
                //save default variable ordering 
                var_order.resize(num_vars + 1); 
                std::iota(var_order.begin(), var_order.end(), 0); 
            }

            /* verbose 
            MEDDLY::FILE_output meddlyout(stdout); 
            domain->showInfo(meddlyout); */
        }
    public:
        //initialize a variable ordering object of n variables (bounds and order are not given)
        inline VariableOrdering(unsigned n_variables) : bounds(n_variables) {
        }

        //copy constructor
        inline VariableOrdering(const VariableOrdering& vo) 
        : bounds(vo.bounds), var_order(vo.var_order) {
            init_meddly_domain();
        } 

        /* initialize a default variable ordering given the domain of the variables; 
         * bounds[i] is the domain of i-th variable */
        inline VariableOrdering(const domain_bounds_t& bounds) : bounds(bounds) {
            init_meddly_domain(); 
        } 

        ~VariableOrdering() { 
            MEDDLY::destroyDomain(domain); 
        }

        inline void set_forest(MEDDLY::forest* forest) {
            this->forest = static_cast<MEDDLY::expert_forest*>(forest);
            impose_variable_ordering();  
        }

        inline void get(var_order_t& ordering) const {
            ordering.resize(size() + 1);
            forest->getVariableOrder(ordering.data()); 
        }

        inline void set(const var_order_t& ordering) {
            forest->reorderVariables(ordering.data());
            var_order = ordering; //update variable ordering 
        }

        inline void impose_variable_ordering() {
            if (!var_order.empty()) {
                forest->reorderVariables(var_order.data());
            }
        }

        // return the number of variables 
        inline size_t size() const {    return bounds.size(); }

        //copy a labelled path and its starting node in @dest following the variable ordering 
        inline void copy_variables(
                const std::vector<node_label_t>& labelled_path, 
                const int graph_node_id,
                std::vector<int>& dest) const {

            std::copy(labelled_path.begin(), labelled_path.end(), dest.begin() + 1); 
            dest.back() = graph_node_id;
        }  

        friend std::ostream& operator << (std::ostream& out, VariableOrdering& ordering) {
            for (int i = 0; i < ordering.bounds.size(); ++i)
                out << ordering.bounds.at(i) << " " << ordering.var_order.at(i+1) << "\n"; 
            
            return out; 
        }

        inline void read(std::ifstream& in) {
            var_order.resize(bounds.size());

            for (int i = 0; i < bounds.size(); ++i) 
                in >> bounds.at(i) >> var_order.at(i);

            var_order.insert(var_order.begin(), 0); 
        }

        void show() const {
            std::cout << "Bounds: "; 
            for (auto x: bounds)       std::cout << x << " ";
            std::cout << "\nOrder: ";
            for (auto x: var_order)    std::cout << x << " ";
            std::cout << std::endl;             
        }
    }; 



    /** GraphNodeEncoder maps graph nodes to a single numeric value */
    class GraphNodeEncoder : public std::map<graph_node_t, unsigned> {
        using base = std::map<graph_node_t, unsigned>; 
        using gn_pair = std::pair<graph_node_t, unsigned>;
        
        //easy way to know the number of vertex of each indexed graph 
        std::map<graph_id_t, size_t> nodes_per_graph; 
        //mapping from integers to the proper pair (graph_id, node_id)
        std::vector<graph_node_t> inverse_mapping; 
        //total number of nodes in the indexed graphs 
        size_t tot_nodes; 

    public: 
        
        GraphNodeEncoder() : base(), tot_nodes(0) {}
        ~GraphNodeEncoder () {}

        void show() const { 
            for (auto it = begin(); it != end(); ++it) {
                std::cout << "(" << it->first.first << " " << it->first.second << ") => " << it->second << std::endl; 
            } 
        }

        inline const graph_node_t& inverse_map(unsigned vmapped) const {
            return inverse_mapping.at(vmapped); 
        }

        //it maps the node nid of graph gif into a unique integer value 
        inline unsigned map(graph_id_t gid, node_id_t nid) { 
            graph_node_t gn(gid, nid);
            base::iterator it = this->find(gn);

            if (it == end()) {
                it = this->insert(std::make_pair(gn, size() + 1)).first; 
                //increment nodes_per_graph counter 
                auto curr = nodes_per_graph.emplace(
                    std::piecewise_construct, 
                    std::forward_as_tuple(gid), 
                    std::forward_as_tuple(0)
                ); 
                ++curr.first->second; 
                ++tot_nodes; 
            }
            
            return it->second;
        } 

        //it returns the integer representing one node of a specific graph 
        inline unsigned get(graph_id_t gid, node_id_t nid) const {
            graph_node_t gn(gid, nid); 
            base::const_iterator it = this->find(gn); 
            return it != end() ? it->second : 0; 
        }

        //it returns the number of nodes of the specified graph  
        inline size_t num_nodes(graph_id_t gid) const {
            auto it = nodes_per_graph.find(gid); 
            return it != nodes_per_graph.end() ? it->second : 0; 
        }

        inline size_t num_graphs() const {
            return nodes_per_graph.size(); 
        }

        //it returns the total number of nodes, considering all the indexed graphs 
        inline size_t num_nodes() const {
            return tot_nodes; 
        }

        inline void build_inverse_mapping() {
            inverse_mapping.resize(size() + 1); 

            for (auto it = begin(); it != end(); ++it)
                inverse_mapping.at(it->second) = it->first;  
        }

        friend std::ostream& operator << (std::ostream& out, GraphNodeEncoder& enc) {
            std::vector<graph_node_t> nodes(enc.size()); 
            std::ostringstream my_text; 

            for (const auto& entry: enc) 
                nodes.at(entry.second - 1) = entry.first;
            
            std::vector<graph_node_t>::iterator 
                it = nodes.begin(), end = nodes.end(); 

            while (it != end) {
                std::ostringstream buffer; 
                int curr_graph_id = it->first; 

                do { 
                    buffer << it->second << " ";
                } while (++it != end && curr_graph_id == it->first); 
                //accumulate string of current graph's nodes
                my_text << buffer.str() << "\n";
            }
            //write the total number of graphs N, then one line for each graph with the enumerated nodes 
            out << enc.nodes_per_graph.size() << "\n"
                << my_text.str();

            return out; 
        }

        void read(std::ifstream& fi) {
            std::stringstream ss; 
            std::string line;

            //read total number of graphs 
            std::getline(fi, line); 
            int num_graphs = std::stoi(line);

            /* read one line for each graph. 
             * The line describes the order in which graph's vertices have been enumerated */ 
            for (int curr_g = 0; curr_g < num_graphs; ++curr_g) {
                std::getline(fi, line);
                ss.str(line); 
                int node_id; 

                while (ss >> node_id) 
                    map(curr_g, node_id);
                
                ss.clear();
            }

            build_inverse_mapping();
        }
    };  


    class Encoder {
        using mapPair = std::pair<std::string, int>; 
        using mapStringToInt = std::map<std::string, int>; 

        mapStringToInt labelMapping; 

    public: 
        inline Encoder() {}

        inline Encoder(GRAPESLib::LabelMap& grapesLabelMap) : Encoder() {
            initFromGrapesLabelMap(grapesLabelMap); 
        }

        inline void initFromGrapesLabelMap(GRAPESLib::LabelMap& grapesLabelMap) {
            for (const auto& x: grapesLabelMap) 
                set(x.first, x.second + 1); 
        }

        inline void initGrapesLabelMap(GRAPESLib::LabelMap& grapesLabelMap) const {
            for (const auto& x: labelMapping) {
                grapesLabelMap.emplace(
                    std::piecewise_construct, 
                    std::forward_as_tuple(x.first), 
                    std::forward_as_tuple(x.second - 1)
                ); 
            }
        }

        inline int map(const std::string& key) {
            auto it = labelMapping.find(key); 

            if (it == labelMapping.end()) 
                //adding +1 in order to avoid zero value 
                it = labelMapping.emplace(key, 1 + labelMapping.size()).first; 

            return it->second; 
        }

        inline int get(const std::string& key) {
            auto it = labelMapping.find(key); 
            return it != labelMapping.end() ? it->second : 0;   
        }

        inline void set(const std::string& key, int value) {
            labelMapping.emplace(key, value); 
        }

        inline bool contains(const std::string& key) {
            return labelMapping.find(key) != labelMapping.end();
        }

        inline mapStringToInt::const_iterator begin() {
            return labelMapping.begin(); 
        } 

        inline mapStringToInt::const_iterator end() {
            return labelMapping.end(); 
        } 

        inline size_t size() const {
            return labelMapping.size();  
        }

        inline void show_mapping() {
            for (auto it = labelMapping.begin(); it != labelMapping.end(); ++it)
                std::cout << it->first << " => " << it->second << "; "; 
            std::cout << std::endl; 
        }

        friend std::ostream& operator << (std::ostream& out, Encoder& enc) {
            std::vector<std::string> labels(enc.size());
            for (const auto& x: enc)
                labels.at(x.second - 1).assign(x.first); 
            for (const auto& x: labels)
                out << x << "\n";
            return out; 
        }


        void read(std::ifstream& fi, int nlabels) {
            for (std::string label; nlabels > 0; --nlabels) {
                getline(fi, label);

                if (label.size() == 0) {
                    ++nlabels;
                    continue;
                }
                map(label);
            }
        }        
    }; 


    struct StatsDD {
        mtmdd::var_order_t ordering; 
        long num_vars = 0; //number of mtdd levels
        long num_graphs = 0;  //number of indexed graphs
        long num_labels = 0;  //number of labels
        long num_nodes = 0; //current number of nodes 
        long peak_nodes = 0; //peak number of nodes reached during mtdd building 
        long memory_used = 0; //current memory required to store mtdd 
        long peak_memory = 0;  //peak memory utilization reached during mtdd building
        long num_edges = 0;  //current number of edges 
        long num_unique_nodes = 0; //current number of unique nodes 
        long cardinality = 0; //number of stored elements 

        void show() const {
            std::stringstream ss; 
            ss << ordering.at(0); 
            for (auto it = ordering.begin() + 1; it != ordering.end(); ++it) ss << "," << *it;


            std::cout << "##################\n"
                << "ordering = " << ss.str() << "\n"
                << "num_vars = " << num_vars << "\n"
                << "num_graphs = " << num_graphs << ", num_labels = " << num_labels << "\n"
                << "num_dd_edges = " << num_edges << "\n"
                << "num_dd_nodes = " << num_nodes << "  , peak = " << peak_nodes << "\n"
                // << "num unique nodes = " << num_unique_nodes << "\n"
                
                << "memory_used = " << memory_used << "  , peak = " << peak_memory << "\n"
                << "dd_cardinality = " << cardinality << std::endl; 
        }
    }; 
}


#endif