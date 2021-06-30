#!/usr/bin/env python3 

import argparse
import networkx as nx 
import networkx.algorithms.centrality as nxc



def load_db(filename):
    graph_cls = nx.Graph if filename.endswith(".gfu") else nx.DiGraph
    graphs = list() 

    with open(filename) as gfile:
        while ( header := gfile.readline().strip() ).startswith("#"):
            graphs.append((net := graph_cls(name = header[1:])))
            #read the number of nodes and the node's labels, then creates the nodes
            nnodes = int(gfile.readline())
            labels = [gfile.readline().strip() for _ in range(nnodes)]
            net.add_nodes_from(
                [(node, dict(label=label)) for node, label in enumerate(labels)])
            #read edges and creates those edges in the graph 
            nedges = int(gfile.readline())
            net.add_edges_from(
                [[int(x) for x in gfile.readline().strip().split()] for _ in range(nedges)])

    return graphs 



if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", dest="input_graph", type=str, required=True)
    args = parser.parse_args() 

    graphs = load_db(args.input_graph)

    print(f"I found {len(graphs)} graphs")


    for i, g in enumerate(graphs):
        print(f"Graph #{i}")

        print(f"Degree: {nx.degree(g)}")
        print(f"Degree histogram: {nx.degree_histogram(g)}")
        print(f"Graph density: {nx.density(g)}")

        # print(f"Node centrality: {nxc.degree_centrality(g)}")
        # print(f"Cloneseness centrality: {nxc.closeness_centrality(g)}")
        # print(f"Betweeness: {nxc.betweenness_centrality(g)}")
        # print(f"Eigencentrality: {nxc.eigenvector_centrality(g)}")
        