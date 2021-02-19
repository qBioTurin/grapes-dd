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

#include "matching.hpp"

#include "Options.h"

#include "MatchingManager.h"
#include "MatchingRunner.h"

#include "AttributeComparator.h"
#include "VF2GraphReaders.h"
#include "argedit.h"





void mtdds::QueryPattern::add_path_to_node(int current_node, const LabelledPath& path) {
    //get labelled path  pointer 
    LabelledPathSet::iterator it_path = unique_paths.emplace(path).first; 
    const LabelledPath* label_pointer = &(*it_path); 
    
    //add the current path to the list of starting paths from current_node 
    paths_from_node.emplace(
        std::piecewise_construct, 
        std::forward_as_tuple(current_node), 
        std::forward_as_tuple()
    ).first->second.push_back(label_pointer); 

    //add the current node to the list of nodes from which the current path starts from 
    nodes_from_path.emplace(
        std::piecewise_construct, 
        std::forward_as_tuple(label_pointer), 
        std::forward_as_tuple()
    ).first->second.insert(current_node); 
}


void mtdds::MatchedQuery::match(MEDDLY::dd_edge& qmatches, std::vector<GraphMatch>& final_matches) {
    MEDDLY::forest* forest = query.query_dd->getForest(); 
    MEDDLY::dd_edge& dd_query = *(query.query_dd); 
    const var_order_t& var_order = var_ordering.var_order(); 
    const unsigned query_num_nodes = query.get_num_nodes(); 
    const int node_encoded_index = var_order.back() + 1;  

    std::map<int, GraphMatch> matched_graphs; //maps from graph id to the list of its matchable vertices 

    MEDDLY::enumerator e(qmatches); 
    while (e) {
        //obtain current node from pruned mtmdd 
        const int current_node_encoded_id = e.getAssignments()[node_encoded_index];   
        int current_graph, current_node;  
        
        std::tie(current_graph, current_node) = gn_enc.inverse_map(current_node_encoded_id); 

        //obtain current graph to match 
        GraphMatch& current_matching_graph = matched_graphs.emplace(
            std::piecewise_construct, 
            std::forward_as_tuple(current_graph), 
            std::forward_as_tuple(current_graph, query_num_nodes)
        ).first->second; 

        /** Since each graph vertex can potentially match multiple query vertices, 
         * all the paths starting from the current graph vertex are retrieved. 
         * For each of these paths, we obtain the possible query vertices from which they start.  */
        std::map<int, int> node_support; 

        do {
            //get starting path from the current node in the pruned mtmdd
            const int *current_dd_path = e.getAssignments(); 
            
            //get query nodes from which the current path starts
            const LabelledPath& current_path = *(query.unique_paths.find(LabelledPath(current_dd_path + 1, var_order)));
            const std::set<int>& candidate_query_nodes(query.find_starting_nodes(current_path));

            //update node support using candidate query nodes 
            for (std::set<int>::const_iterator it = candidate_query_nodes.begin(); it != candidate_query_nodes.end(); ++it) {
                std::pair<std::map<int, int>::iterator, bool> it_map = node_support.emplace(*it, 1); 
                if (!it_map.second)
                    ++(it_map.first->second); 
            }
            ++e;
        } while (e && e.getAssignments()[node_encoded_index] == current_node_encoded_id);

        for (auto it = node_support.begin(); it != node_support.end(); ++it) {
            int current_query_node = it->first; 
            const std::vector<const LabelledPath*>& paths_from_query_node(query.find_starting_paths(current_query_node)); 
             

            if (it->second >= paths_from_query_node.size()) {
                //for each path starting from the current query node, we have to check
                //1. if the path also starts from the matched vertex
                //2. if the occurrence number of the matched vertex is NOT LESS than the occurrence number of the query 
                bool matched_node_flag = true; 

                for (const LabelledPath* query_path: paths_from_query_node) {
                    int* buffer_entry = query_path->get_pointer2buffer(); 
                    int query_n_occ = query_path->get_occurrence_number(), vertex_n_occ; 

                    buffer_entry[node_encoded_index] = current_node_encoded_id;  
                    forest->evaluate(qmatches, buffer_entry, vertex_n_occ); 

                    if (vertex_n_occ / query_n_occ < query_n_occ) {
                        matched_node_flag = false; 
                        break; 
                    }
                }

                if (matched_node_flag) {
                    current_matching_graph.add_match(current_query_node, current_node); 
                }
            }  
        }
    }


    for (auto x = matched_graphs.begin(); x != matched_graphs.end(); ++x)
        if (x->second.is_complete_match()) {
            final_matches.emplace_back(x->second); 
        } 
}


void mtdds::GraphMatch::get_node_cands(GRAPESLib::node_cands_t& ncands) const {
    int q_index = 0;

    for (auto it = begin(); it != end(); ++it, ++q_index) {
        sbitset s; //candidate nodes for the current query graph 

        for (auto it_q = it->begin(); it_q != it->end(); ++it_q) 
            s.set(*it_q, true); 
        
//        std::cout << "target #" << graph_id << ", q_node #" << q_index << " ==> " << s.count_ones() << std::endl; 

        ncands.emplace(
            std::piecewise_construct, 
            std::forward_as_tuple(q_index), 
            std::forward_as_tuple(s)
        ); 
    }
}


void mtdds::graph_find(
        const std::string& input_network_file, 
        const std::string& query_graph_file,  
        bool direct_flag,
        int nthreads, 
        const Encoder& labelMapping,
        const std::vector<GraphMatch>& matched_vertices, 
        std::map<std::string, double>& match_stats) {

    time_point dbload_t, balance_t, match_t; 
    double load_db_time, decomposing_time, matching_time, total_time; 

    GRAPESLib::LabelMap labelMap; 
    labelMapping.initGrapesLabelMap(labelMap);

    GRAPESLib::filtering_graph_set_t fgset; //set containing the ids of candidate graphs 
    GRAPESLib::graph_node_cands_t gncands; //maps containing a lot of useful stuff 

    for (auto it = matched_vertices.begin(); it != matched_vertices.end(); ++it) {
        GRAPESLib::node_cands_t q_nodes;
        //adding information about nodes of the target graph matching query nodes         
        it->get_node_cands(q_nodes); 

        gncands.emplace(
            std::piecewise_construct, 
            std::forward_as_tuple(it->graph_id), 
            std::forward_as_tuple(q_nodes)
        );

        //adding candidate graph id 
        fgset.insert(it->graph_id);
    }

    // load graph collection 
    dbload_t = std::chrono::_V2::steady_clock::now(); 

    GRAPESLib::VF2GraphReader_gfu  s_q_reader(labelMap);
	s_q_reader.direct = direct_flag;
	s_q_reader.open(query_graph_file);
	ARGEdit* squery;
	if((squery = s_q_reader.readGraph()) == NULL){
		std::cout<<"Cannot open query file for matching\n";
		exit(1);
	}
	s_q_reader.close();

    std::map<graph_id_t, GRAPESLib::ReferenceGraph*> rgraphs;
	GRAPESLib::VF2GraphReader_gfu  s_r_reader(labelMap);
	s_r_reader.direct = direct_flag;
	s_r_reader.open(input_network_file);
	graph_id_t srid = 0;
	bool sreaded = true;

	do{
		//ARGEdit* rgraph = s_r_reader.readGraph();
		GRAPESLib::VF2Graph* rgraph = s_r_reader.readSGraph();
		sreaded = (rgraph != NULL);
		if(sreaded){
			rgraphs.insert(std::pair<graph_id_t, GRAPESLib::ReferenceGraph* >(srid, rgraph));
			++srid;
		}
	} while(sreaded); 

    load_db_time = get_time_interval(std::chrono::_V2::steady_clock::now(), dbload_t);
    balance_t = std::chrono::_V2::steady_clock::now(); 

    /*
	MPRINT_OPT_NO 		= 0,
	MPRINT_OPT_CONSOLE 	= 1,
	MPRINT_OPT_FILE 	= 2	 */
    MPRINT_OPTIONS mprint_opt = MPRINT_OPT_NO;

    GRAPESLib::QueryGraph aqg(squery);   
	GRAPESLib::MatchingManager mman(aqg, rgraphs, fgset, gncands, *(new GRAPESLib::DefaultAttrComparator()), nthreads);
	GRAPESLib::MatchRunner mrunner(mman, nthreads, squery->NodeCount(), mprint_opt);
	mrunner.runInitPhase();
	std::map<int, std::list<GRAPESLib::g_match_task_t> > assign;

	mman.createBalancedRun(); 



    decomposing_time = get_time_interval(std::chrono::_V2::steady_clock::now(), balance_t); 
    match_t = std::chrono::_V2::steady_clock::now(); 

    mrunner.runMatch(rgraphs, *squery);

    matching_time = get_time_interval(std::chrono::_V2::steady_clock::now(), match_t); 
    total_time = matching_time + decomposing_time + load_db_time; 

    std::set<graph_id_t> candidates;
    for(std::list<GRAPESLib::g_match_task_t>::iterator IT = mman.coco_units.begin(); IT!=mman.coco_units.end(); IT++)
	    candidates.insert((*IT).first);

    int nof_cocos = 0;
    long nof_matches = 0;
    for(thread_id_t i = 0; i  < nthreads; i++){
        nof_cocos += mman.number_of_cocos[i];
        nof_matches += mman.number_of_matches[i];
    }

    std::set<graph_id_t> matching_graphs;
    for(thread_id_t i=0; i < nthreads; i++)
	    matching_graphs.insert(mman.matching_graphs[i].begin(), mman.matching_graphs[i].end());

    match_stats.insert(std::pair<std::string, double>("load_db_time", load_db_time)); 
    match_stats.insert(std::pair<std::string, double>("decompose_time", decomposing_time)); 
    match_stats.insert(std::pair<std::string, double>("matching_time", matching_time)); 
    match_stats.insert(std::pair<std::string, double>("tot_matching_time", total_time)); 
    match_stats.insert(std::pair<std::string, double>("n_cand_graphs", candidates.size())); 
    match_stats.insert(std::pair<std::string, double>("n_cocos", nof_cocos)); 
    match_stats.insert(std::pair<std::string, double>("n_matching_g", matching_graphs.size())); 
    match_stats.insert(std::pair<std::string, double>("n_found_m", nof_matches)); 
    
    
}