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

#ifndef MATCHING_HPP
#define MATCHING_HPP 


#include <vector>
#include <set>
#include <map>
#include <iterator>
#include <meddly.h>

#include "single_buffer.hpp"
#include "dd_utils.hpp"

#include "OCPTreeListeners.h"
#include "typedefs.h"


namespace mtdds {
    class LabelledPath; 
    class QueryPattern; 
    class MatchedQuery; 
    class GraphMatch; 


    class LabelledPath : public std::vector<node_label_t> {
        using base = std::vector<node_label_t>;
        
        int* buffer_location = nullptr;  //points to the buffer entry representing the same path 
        int occurrence_number = -1; 

    public:
        inline LabelledPath() : base() {}

        inline LabelledPath(const int* vpath, const var_order_t& var_ordering)
        : base(var_ordering.size() - 1) {
            int index = 0; 

            for (auto it = begin(); it != end(); ++it, ++index) 
                *it = vpath[var_ordering[index]]; 
        }

        inline LabelledPath(const base& path) 
            : base(path), buffer_location(nullptr) {} 

        inline LabelledPath(const LabelledPath& labelPath) 
            : base(labelPath), buffer_location(labelPath.buffer_location), occurrence_number(labelPath.occurrence_number) {}

        inline void assign_pointer2buffer(int* buffer_slot) {
            if (buffer_location == nullptr) {
                buffer_location = buffer_slot; 
            }
        }

        inline void set_occurrence_number(unsigned n_occ) {
            occurrence_number = n_occ; 
        }

        inline int get_occurrence_number() const {
            return occurrence_number; 
        }

        inline int* get_pointer2buffer() const {
            return  buffer_location; 
        }

        inline void print() const {
            for (auto it = begin(); it != end(); ++it)
                std::cout << *it << " "; 
            std::cout << std::endl; 
        }
    }; 

    class QueryPattern {
    public: 
        using LabelledPathSet = std::set<LabelledPath>; 
        //map from int(node_id) to a vector of paths (the paths starting from node_id)
        using Node2PathsMapping = std::map<int, std::vector<const LabelledPath*> >; 
        //map from a path to the set of node_id from which the key path starts 
        using Path2NodesMapping = std::map<const LabelledPath*, std::set<int> >;

    public:
    ////////////////private
        //set of all paths present in the query 
        LabelledPathSet unique_paths; 
        //mapping from node_ids to the list of paths starting from that node 
        Node2PathsMapping paths_from_node; 
        //mapping from paths to the list of query nodes from which that path starts
        Path2NodesMapping nodes_from_path; 

        
        MEDDLY::dd_edge* query_dd = nullptr; 
    //////////////////private

        inline void assign_dd_edge(MEDDLY::dd_edge* q_edge) { 
            if (!query_dd)
                query_dd = q_edge; 
        }

        void add_path_to_node(int node_id, const LabelledPath& path); 

        inline const std::set<int>& find_starting_nodes(const LabelledPath& input_path) const {
            LabelledPathSet::const_iterator it_set = unique_paths.find(input_path);
            Path2NodesMapping::const_iterator it_nodes = nodes_from_path.find(&(*it_set)); 
            return it_nodes->second; 
        }

        inline const std::vector<const LabelledPath*>& find_starting_paths(const int qnode_id) const {
            Node2PathsMapping::const_iterator it = paths_from_node.find(qnode_id); 
            return it->second; 
        }

        inline const size_t get_num_nodes() const {
            return paths_from_node.size(); 
        }


        void show() const {
            for (auto it = paths_from_node.begin(); it != paths_from_node.end(); ++it) {
                std::cout << "node #" << it->first << ": " << it->second.size() << " paths from it;\n";
                for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
                    const LabelledPath *lp = *it2;
                    for (auto x = lp->begin(); x != lp->end(); ++x)
                        std::cout << *x << " "; 
                    std::cout << "\n"; 
                }
            }
        } 
    };

    class MatchedQuery {
        const QueryPattern& query; 
        const GraphNodeEncoder& gn_enc; 
        const VariableOrdering& var_ordering;
    public:
        MatchedQuery(const QueryPattern& query, const GraphNodeEncoder& gn_enc, const VariableOrdering& var_ordering) 
            : var_ordering(var_ordering), query(query), gn_enc(gn_enc) {}

        void match(MEDDLY::dd_edge& qmatches, std::vector<GraphMatch>& final_matches); 
    }; 

    class GraphMatch : private std::vector< std::set<unsigned> > {
        //list of matchable vertices of a query node 
        using u_set = std::set<unsigned>; 
        //list of matchable vertices for each vertex of the query 
        using base = std::vector<u_set>;

    public:
        const unsigned graph_id; 

        inline GraphMatch(unsigned graph_id, unsigned n_query_nodes) 
            : base(n_query_nodes), graph_id(graph_id) {}

        inline GraphMatch(const GraphMatch& gm) 
            : base(gm), graph_id(gm.graph_id) {}

        inline void add_match(unsigned query_node_id, unsigned graph_node_id) {
            at(query_node_id).insert(graph_node_id); 
        }

        void get_node_cands(GRAPESLib::node_cands_t& ncands) const; 

        inline bool is_complete_match() const {
            for (const u_set& curr_set: *this)
                if (curr_set.size() == 0)
                    return false; 
            return true; 
        }

        inline void report_match() const {
            int index = 0; 
            std::cout << "Reporting matches for graph #" << graph_id << std::endl; 
            for (auto it = begin(); it != end(); ++it, ++index) {
                std::cout << "node #" << index << ": ";
                for (auto it2 = it->begin(); it2 != it->end(); ++it2)
                    std::cout << *it2 << " "; 
                std::cout << "\n"; 
            }
        }

        inline bool operator < (const GraphMatch& gm) const {
            return graph_id < gm.graph_id; 
        }

        inline bool operator == (const GraphMatch& gm) const  {
            return graph_id == gm.graph_id; 
        }
    }; 



    void graph_find(
        const std::string& input_network_file, 
        const std::string& query_graph_file,  
        bool direct_flag,
        int nthreads, 
        const Encoder& labelMapping,
        const std::vector<GraphMatch>& matched_vertices, 
        std::map<std::string, double>& match_stats); 
}

#endif //MATCHING_HPP