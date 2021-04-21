#!/bin/bash 

export LD_LIBRARY_PATH="/opt/Meddly/meddly-0.15.0/src/.libs"

function assert {
    rvalue="${1}"

    if [[ ${rvalue} != "0" ]]; then
        echo "Abort. Some error has occurred."
        exit 1
    fi 
}


graph_db="${1}"
max_depth="${2}"

file_data_sample="${graph_db}_samples.txt"

#1. run entropy.cpp without order in order to extract a sample of paths 
./entropy -i ${graph_db} --out ${file_data_sample}  -l ${max_depth} 

assert $?

if false; then

    #2. run entropy.py over the path sample in order to get a possible variable ordering 
    echo "Running heuristic..."
    order=`./entropy.py --data ${file_data_sample}`

    assert $?

    echo -e "Applying ordering => ${order}"
    ./entropy -i ${graph_db} --out ${file_data_sample} -l ${max_depth} -v ${order}

    assert $? 
fi


echo "Test completed without errors."