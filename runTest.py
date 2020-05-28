#!/usr/bin/env python3 

# This script allows you to compare GRAPES and GRAPES-DD over the dataset provided in the ./example/ folder,
# which contains both real and synthetic graphs, as well as the query graphs. 
# We recommend you to install Docker before run this script, in order to run the tools in a container with all the 
# software and libraries required. 
# Otherwise, you have to install Meddly Library v0.15.0 from https://github.com/asminer/meddly/releases before run this script. 

import argparse
import os 
import subprocess
import sys 

tool_path = {
    "grapes": os.path.realpath("./src/GRAPES"),
    "grapes_dd": os.path.realpath("./src")
}


def check_command(command):
    print("Checking for '{}' command: ".format(command), end="", flush=True)
    cmd = "command -v {}".format(command)
    returncode = subprocess.run(cmd, shell=True).returncode == 0
    print(("" if returncode else "not present."))
    return returncode 


def build_docker():
    print("Building docker image...")
    cmd = """docker build -t "grapes_dd" ."""
    subprocess.run(cmd, shell=True)

def build_toolz():
    grapes_exec = os.path.join(tool_path["grapes"], "grapes")
    grapes_dd_exec = os.path.join(tool_path["grapes_dd"], "grapes_dd")

    cmd_grapes = "cd {}; make -B".format(tool_path["grapes"])
    cmd_grapes_dd = "cd {}; make grapes_dd".format(tool_path["grapes_dd"])

    if not os.path.exists(grapes_exec) and subprocess.run(cmd_grapes, shell=True).returncode != 0:
        print("GRAPES building failed.")
        return False
    if not os.path.exists(grapes_dd_exec) and subprocess.run(cmd_grapes_dd, shell=True).returncode != 0:
        print("GRAPES-DD building failed.")
        return False
    return True 

def get_direct_flag(db_name):
    return "true" if db_name.endswith(".gfd") else "false"


def run_docker_indexing(tool, db_pathname, lp):
    db_path, db_name = os.path.split(os.path.realpath(db_pathname))
    cmd = "docker run -v {}:/data/db_file grapes_dd {} -i {} -l {} -d {}".format(
        db_path, tool, db_name, lp, get_direct_flag(db_name)
    )
    return cmd 

def run_docker_query(tool, db_pathname, query_pathname, lp, nthreads):
    db_path, db_name = os.path.split(os.path.realpath(db_pathname))
    query_path, query_name = os.path.split(os.path.realpath(query_pathname))
    cmd = "docker run --user=$UID -v {}:/data/db_file -v {}:/data/query_file grapes_dd {} -i {} -q {} -l {} -d {} -t {}".format(
        db_path, query_path, tool, db_name, query_name, lp, get_direct_flag(db_name), nthreads
    )
    return cmd

def run_indexing(tool, db_pathname, lp):
    path_tool = os.path.join(tool_path[tool], tool)
    db_ext = db_pathname.split(".")[-1]
    cmd = "{} 1 -b -{} {} -lp {}".format(path_tool, db_ext, db_pathname, lp) \
        if tool == "grapes" \
        else "{} -i {} -l {} -d {}".format(path_tool, db_pathname, lp, get_direct_flag(db_pathname))
    return cmd

def run_query(tool, db_pathname, query_pathname, lp, nthreads):
    path_tool = os.path.join(tool_path[tool], tool)
    db_ext = db_pathname.split(".")[-1]
    cmd = "{} {} -f -{} {} {} -no -lp {}".format(path_tool, nthreads, db_ext, db_pathname, query_pathname, lp) \
        if tool == "grapes" \
        else "{} -i {} -q {} -l {} -d {} -t {}".format(path_tool, db_pathname, query_pathname, lp, get_direct_flag(db_pathname), nthreads)
    return cmd

def execute_command(cmd):
    ret = subprocess.run(cmd, shell=True)
    return ret.returncode 

def print_summary(db_name):
    db_path, db_name = os.path.split(os.path.realpath(db_name)) 
    mem_file = os.path.join(db_path, "memory_peak.stat")

    if os.path.exists(mem_file):
        data = list() 
        with open(mem_file) as mf: 
            tool, peak = None, None 
            for line in mf: 
                if line.startswith("/usr/bin/time"):
                    peak = line.split(":")[-1].strip()
                    data.append((tool, peak))
                else:
                    tmp_tool = line.split()[0]
                    if tmp_tool == "grapes":
                        tool = "grapes   ", "indexing"
                    elif tmp_tool == "grapes_query":
                        tool = "grapes   ", "matching"
                    elif tmp_tool == "mtmdd":
                        tool = "grapes_dd", "indexing"
                    elif tmp_tool == "mtmdd_query":
                        tool = "grapes_dd", "matching"
                    else:
                        print("Unrecognized line in memory_peak.stat")

        
        print("Tool\t\tOperation\t\tPeak in RAM (in KB)")
        for tool, peak in data: 
            print("{}\t\t{}\t\t{}".format(tool[0], tool[1], peak))
                    
        os.remove(mem_file)
    

if __name__ == "__main__":
    parser = argparse.ArgumentParser("GRAPES-DD test utility")
    parser.add_argument("-i", "--input", dest="db_name", type=str)
    parser.add_argument("-q", "--query", dest="query_name", type=str)
    parser.add_argument("-l", "--lp", dest="max_pathlength", default=4, type=int)
    parser.add_argument("-t", "--threads", dest="num_threads", default=1, type=int)
    args = parser.parse_args()

    tools = ("grapes", "grapes_dd")

    if args.db_name is None:
        args.db_name = "example/queries/synthetic/Barabasi/directed/10000/6/target-power1.5-2_1.gfd"
        args.query_name = "example/queries/synthetic/Barabasi/directed/10000/6/target-power1.5-2_1/4/1_sub_3.gfd"

    print(
"""
This script allows you to compare GRAPES and GRAPES-DD over the dataset provided in the ./example/ folder,
which contains both real and synthetic graphs, as well as the query graphs. 
We recommend you to install Docker before run this script, in order to run the tools in a container with all the 
software and libraries required. 
Otherwise, you have to install Meddly Library v0.15.0 from https://github.com/asminer/meddly/releases before run this script. 
""")

    input("\nPress ENTER to start...\n")

    if check_command("docker"):
        build_docker()

        for tool in tools:
            print("\n\n\t\tBuilding indexing with {}\n\n".format(tool.upper()))
            cmd = run_docker_indexing(tool, args.db_name, args.max_pathlength)
            execute_command(cmd)
            input("\nBuilding indexing with {} finished.\n\nPress ENTER to continue... ".format(tool.upper()))

        if args.query_name is not None:
            for tool in tools:
                print("\n\n\t\tQuery matching with {}\n\n".format(tool.upper()))
                cmd = run_docker_query(tool, args.db_name, args.query_name, args.max_pathlength, args.num_threads)
                execute_command(cmd)
                input("\nQuery matching with {} finished.\n\nPress ENTER to continue... ".format(tool.upper()))
        
        print_summary(args.db_name)
    else: 
        print("We recommend you to install Docker and then run this script.\n")
        time_flag = check_command("/usr/bin/time")
        if time_flag:
            print("Peak in RAM measured in KB will be shown immediately below the 'total time' field.")
        else:
            print("Please install time command in order to show peak in RAM.\nOtherwise, please install Docker.\n")
            
        input("\nPress ENTER to continue... ")
        
        if build_toolz():
            for tool in tools:
                print("\n\n\t\tBuilding indexing with {}\n\n".format(tool.upper()))
                cmd = run_indexing(tool, args.db_name, args.max_pathlength)
                if time_flag:
                    cmd = "/usr/bin/time -f '%M' {}".format(cmd)
                execute_command(cmd)
                input("\nBuilding indexing with {} finished.\n\nPress ENTER to continue... ".format(tool.upper()))

            if args.query_name is not None: 
                for tool in tools:
                    print("\n\n\t\tQuery matching with {}\n\n".format(tool.upper()))
                    cmd = run_query(tool, args.db_name, args.query_name, args.max_pathlength, args.num_threads)
                    if time_flag:
                        cmd = "/usr/bin/time -f '%M' {}".format(cmd)
                    execute_command(cmd)
                    input("\nQuery matching with {} finished.\n\nPress ENTER to continue... ".format(tool.upper()))

            print_summary(args.db_name)
        else:
            print("Building failed. We suggest you to install Docker and then run this script.", file=sys.stderr)