# GRAPES-DD
> GRAPES-DD is a modified version of GRAPES (available at https://github.com/InfOmics/GRAPES ) in which the trie indexing structure has been replaced with a multi-terminal decision diagram (MTMDD). 

<hr/>

### Description 

GRAPES-DD is developed in C++ under GNU\Linux using POSIX Threads programming.
It requires the MEDDLY Library 0.15.0 available at https://github.com/asminer/meddly/releases

The GRAPES-DD workflow is composed by two main phases:
1. the indexing building phase in which an MTMDD indexing the collection of target graphs is created;
2. the filtering phase in which, given a query graph, the set of target graphs is potentially restricted to those sub-graphs probably containing the query.

The GRAPES-DD verification phase remains as in the original version of the software.

<hr/>

### Usage

GRAPES-DD is developed in C++ under GNU\Linux using POSIX Threads programming.
It requires the MEDDLY Library 0.15.0 available at https://github.com/asminer/meddly/releases

The executable *grapes_dd* allows to both build the database index and to run a query. 

##### Build from source code 

Executables are available only after building source code on your system.

```
cd src/GRAPES && make -B 
cd ..
make clean
make grapes_dd
```

##### Database Index Construction

Build the index of the given database of graphs.
```
./grapes_dd -i db_file -l lp -d bool 
```

| Parameter | Description |
|-----------------------|-------------|
|**-i db_file**| textual graphs database file|
|**-l lp**| specify feature paths length, namely the depth of the DFS which extract paths. lp must be greather than 1, eg -lp 3. Default value is 4.
|**-d bool**| flag indicating if the graphs are directed (true) or undirected (false). Default value is true.

The indexing phase  produces the *db_file.index.lp.mtdd* file in which the database index is stored.

##### Querying
```
./grapes_dd -i db_file -q query_file -l lp -d bool -t nthreads
```

| Attribute | Description |
|-----------------------|-------------|
|**-i db_file** | textual graphs database file|
|**-q query_file** | textual query graph file. It must contain just one graph |
|**-l lp**| specify feature paths length, namely the depth of the DFS which extract paths. lp must be greather than 1, eg -lp 3. Default value is 4. 
|**-d bool**| flag indicating if the graphs are directed (true) or undirected (false). Default value is true. 
|**-t nthreads**| number of threads to be used during matching phase 

<span style="color:red">ATTENTION:</span> before run a query, the database index must have been computed and the resultant file must be maintained in the same directory of the database textual file.


<hr/>

### Use containerized GRAPES-DD 

We built a Docker image containing both GRAPES-DD and GRAPES.

##### Build the docker image

```docker build -t "docker_user/grapes_dd" .```

##### Database Index Construction 

You have to mount the folder containing the graph database in ```/data/db_folder```.

```docker run -v db_folder:/data/db_file docker_user/grapes_dd [mode] -i db_file -l lp -d bool```


| Parameter | Description |
|-----------------------|-------------|
|**mode**| specify the tool to be used (grapes or grapes_dd). 
|**-i db_file**| textual graphs database file|
|**-l lp**| specify feature paths length, namely the depth of the DFS which extract paths. lp must be greather than 1, eg -lp 3. Default value is 4.
|**-d bool**| flag indicating if the graphs are directed (true) or undirected (false). Default value is true.


##### Querying

You have to mount the folders containing the graph database and the query file in ```/data/db_file``` and ```/data/query_file```, respectively. 

```docker run -v db_folder:/data/db_file -v query_folder:/data/query_file docker_user/grapes_dd [grapes|grapes_dd] -i db_file -l lp -d bool -t nthreads```


| Parameter | Description |
|-----------------------|-------------|
|**mode**| specify the tool to be used (grapes or grapes_dd). 
|**-i db_file**| textual graphs database file|
|**-q query_file** | textual query graph file. It must contain just one graph |
|**-l lp**| specify feature paths length, namely the depth of the DFS which extract paths. lp must be greather than 1, eg -lp 3. Default value is 4.
|**-d bool**| flag indicating if the graphs are directed (true) or undirected (false). Default value is true.
|**-t nthreads**| number of threads to be used during matching phase 



