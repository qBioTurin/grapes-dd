# GRAPES-DD
> GRAPES-DD is a modified version of GRAPES (available at https://github.com/InfOmics/GRAPES ) in which the trie indexing structure has been replaced with a multi-terminal multiway decision diagram (MTMDD). 

GRAPES-DD research paper has been published on BMC Bioinformatics: it is available [here](https://bmcbioinformatics.biomedcentral.com/articles/10.1186/s12859-021-04129-0).
<hr/>

### Description 

GRAPES-DD is developed in C++ under GNU\Linux using POSIX Threads programming.
It requires the MEDDLY Library 0.15.1 available at https://github.com/asminer/meddly/releases

The GRAPES-DD workflow is composed by two main phases:
1. the indexing building phase in which an MTMDD indexing the collection of target graphs is created;
2. the filtering phase in which, given a query graph, the set of target graphs is potentially restricted to those sub-graphs probably containing the query.

The GRAPES-DD verification phase remains as in the original version of the software.

<hr/>

### Datasets

We provide all the graphs used during testing.

The directory ```example/graphs``` provides the collections of graphs. 
The directory ```example/queries``` provides both the collections of graphs and the query graphs.
Inside these directory there are two subdirectories: 
* ```biochemical``` directory contains the biochemical graphs: PDBS, PCM and PPI, as well as the single PPI networks (C. elegans, D. melanogaster, H. sapiens, M. musculus and S. cerevisiae).  
* ```synthetic``` directory contains the synthetic Barabasi and Forestfire graphs. 
    * Barabasi folder structure is the following: ```Barabasi/directed/num_nodes/degree/target-powerP-ID_LabelRange.gfd``` where:
        * num_nodes is the number of vertices of the graph
        * degree is the average degree of a vertex 
        * P is the exponent of the power-law 
        * LabelRange is the percentage of distinct labels respect to the number of vertices. 
    * ForestFire folder structure is the following: ```ForestFire/directed/num_nodes/p/targetID_LabelRange.gfd``` where:
        * num_nodes is the number of vertices of the graph
        * p is the probability of a link between two vertices
        * LabelRange is the percentage of distinct labels respect to the numer of vertices. 

Each target graph presents in the ```queries``` folder has a dedicated subfolder containing the query graphs extracted from it. Query folder names represent the number of vertices of the query graph. Query graph names are ```LabelRange_sub_ID.gfd```. 

### Usage

GRAPES-DD is developed in C++ under GNU\Linux using POSIX Threads programming.
It requires the MEDDLY Library 0.15.1 available at https://github.com/asminer/meddly/releases

The executable *grapes_dd* allows you to both build the database index and to run a query. 

You can try and compare GRAPES-DD and GRAPES performances by using the runTest.py python script. 
Tests will be run through Docker if it is installed.

Index building phase: 

```./runTest.py [-i folder_database/graph_database.gfd] -l lp -t num_threads```

Index building + query matching phase:

```./runTest.py [-i folder_database/graph_database.gfd -q folder_database/folder_query/query_graph.gff] -l lp -t nt```


| Parameter | Description |
|-----------------------|-------------|
|**-i db_file**| path of the textual graphs database file|
|**-q query_file**|path of the textual graph query file |
|**-l lp**     | specify feature paths length, namely the depth of the DFS which extract paths. lp must be greather than 1, eg -lp 3. Default value is 4.
|**-t nt**   | number of threads to be used during matching phase. Default value is 1. 

We recommend you to run the software via Docker. See https://www.docker.com/

<span style="color:red">ATTENTION:</span> the query graph file must be located in a subdirectory of the graph database file. Query graphs are graph database specific. 

##### Build from source code 

Executables are available only after building source code on your system. 
Use the build_all.sh script to build the grapes_dd executable. 

```
./build_all.sh 
```

##### Database Index Construction

Build the index of the given database of graphs.
```
./grapes_dd -i db_file -l lp -d bool 
```

| Parameter | Description |
|-----------------------|-------------|
|**-i db_file**| textual graphs database file|
|**-l lp**     | specify feature paths length, namely the depth of the DFS which extract paths. lp must be greather than 1, eg -lp 3. Default value is 4.
|**-d bool**   | flag indicating if the graphs are directed (true) or undirected (false). Default value is true.

The indexing phase  produces the *db_file.index.lp.mtdd* file in which the database index is stored.

##### Querying
```
./grapes_dd -i db_file -q query_file -l lp -d bool -t nthreads
```

| Attribute | Description |
|-----------------------|-------------|
|**-i db_file**    | textual graphs database file
|**-q query_file** | textual query graph file. It must contain just one graph 
|**-l lp**         | specify feature paths length, namely the depth of the DFS which extract paths. lp must be greather than 1, eg -lp 3. Default value is 4. 
|**-d bool**       | flag indicating if the graphs are directed (true) or undirected (false). Default value is true. 
|**-t nthreads**   | number of threads to be used during matching phase 

<span style="color:red">ATTENTION:</span> before run a query, the database index must have been computed and the resultant file must be maintained in the same directory of the database textual file.


<hr/>

### Use containerized GRAPES-DD 

We built a Docker image containing both GRAPES-DD and GRAPES. 
In the folder where the Dockerfile is located, run the following command to build the image:

##### Build the docker image

```docker build -t "grapes_dd" .```

##### Database Index Construction 

You have to mount the folder containing the graph database in ```/data/db_folder```.

```docker run -v db_folder:/data/db_file grapes_dd [mode] -i db_file -l lp -d bool```


| Parameter | Description |
|-----------------------|-------------|
|**mode**      | specify the tool to be used (grapes or grapes_dd). 
|**-i db_file**| textual graphs database file|
|**-l lp**     | specify feature paths length, namely the depth of the DFS which extract paths. lp must be greather than 1, eg -lp 3. Default value is 4.
|**-d bool**   | flag indicating if the graphs are directed (true) or undirected (false). Default value is true.


##### Querying

You have to mount the folders containing the graph database and the query file in ```/data/db_file``` and ```/data/query_file```, respectively. 

```docker run -v db_folder:/data/db_file -v query_folder:/data/query_file grapes_dd [grapes|grapes_dd] -i db_file -l lp -d bool -t nthreads```


| Parameter | Description |
|-----------------------|-------------|
|**mode**          | specify the tool to be used (grapes or grapes_dd). 
|**-i db_file**    | textual graphs database file|
|**-q query_file** | textual query graph file. It must contain just one graph 
|**-l lp**         | specify feature paths length, namely the depth of the DFS which extract paths. lp must be greather than 1, eg -lp 3. Default value is 4.
|**-d bool**       | flag indicating if the graphs are directed (true) or undirected (false). Default value is true.
|**-t nthreads**   | number of threads to be used during matching phase 
