#!/usr/bin/env python3 

import argparse
import logging
import numpy as np 
import pandas as pd 
from scipy.stats import contingency, entropy, chi2_contingency, chi2 
from collections import Counter
from itertools import combinations  
import os 
import enum 


# Enable logging
logging.basicConfig(
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s', level=logging.WARNING
)
logger = logging.getLogger(__name__)


class Heuristic(enum.Enum):
    ENTROPY = "entropy_naive"
    CORRELATION = "correlation"
    ENTROPY_COUNTS = "entropy_counts"

    @classmethod
    def get(cls, name: str):
        x = [h for h in cls if h.value == name]
        return x[0] if x else None 


class VariableOrdering:
    def __init__(self, var_ordering, df_data, df_columns) -> None:
        self.ordering = var_ordering
        self.stats = pd.DataFrame(df_data, columns=df_columns)


class Heu:
    def __init__(self, paths_filename: str):
        self.order = None 
        self.unbounded = None 
        self.history = None 
        #init df, counts and variables attributes 
        self.load_dd_data(paths_filename)


    def init_order(self):
        self.order = list() 
        self.unbounded = set(self.variables)
        self.history = None #pd.DataFrame(columns=["variables", "entropy"])

    @property
    def available_heuristics(self):
        return [h.value for h in Heuristic]
        #return #("entropy", "correlation", "entropy_counts")

    def load_dd_data(self, filename, terminal_col = "terminal"):
        self.df = pd.read_csv(filename, sep="\t").astype(str).iloc[1:]

        #get variable names
        self.variables = [col for col in self.df.columns if col != terminal_col]
        self.col_node_id = max(self.variables, key=lambda varname: int(varname.replace("v_", "")))  


    def compute_heuristic(self, which: str):
        maps = {
            Heuristic.ENTROPY.value:        self.__sort_by_entropy, 
            Heuristic.ENTROPY_COUNTS.value: self.__sort_by_entropy2, 
            Heuristic.CORRELATION.value:    self.__chi2_heu
        }

        try:
            return maps[which]()
        except KeyError:
            raise Exception("Invalid heuristic choice")


    def __entropy(self, var):
        vars = self.order + [var]
        df = self.df[vars].apply(lambda seq: tuple(seq), axis=1)
        counts = Counter()

        for x, y in zip(df, self.df["terminal"]):
            counts[x] += float(y)
        
        probs = np.array(list(counts.values())) / sum(counts.values())
        
        return -sum([p*np.log2(p) for p in probs])

        
    

    def __sort_by_entropy2(self):
        self.init_order() 
        history = list() 

        while len(self.unbounded) > 1:
            best_h, best_v = np.infty, None 

            for variable in self.unbounded:
                h = self.__entropy(variable)
                if h < best_h:
                    best_h, best_v = h, variable

                history.append([self.order + [variable], h])
            
            self.order.append(best_v)
            self.unbounded.remove(best_v)

        self.order.append(self.unbounded.pop())
        return VariableOrdering(self.order[::-1], history, ["variables", "entropy_c"])
            

    def __sort_by_entropy(self):
        self.init_order() 
        history = list() 

        while len(self.unbounded) > 1:
            best_h, best_v = np.infty, None 

            #combine each unbounded variable with partial ordered variables
            for variable in self.unbounded:
                vars = self.order + [variable]
                #create kmers combining variables belonging to the partial order and current variable
              #  df = self.df[vars].apply(lambda seq: tuple(seq), axis=1)

                counts = self.df[vars].apply(lambda seq: tuple(seq), axis=1).value_counts() 
                


#                counts = df.value_counts()
                # print(f"Counts {type(counts)}:\n{len(counts)}\n")
                
                #count occurrences and estimate entropy for current set of objects 
            #    kmers, n_occ = np.unique(df, return_counts=True)

               # print(f"Unique: {len(n_occ)} vs {len(counts)}")

#                print(f"Unique: \n{len(list(kmers,n_occ)))}")

                hh = entropy(counts, base=2)
                if hh < best_h:
                    best_h, best_v = hh, variable

                history.append([vars, hh])
                logger.info(f"Entropy for {vars}: {hh}")

            #add variable with minimum entropy to the order 
            self.order.append(best_v)
            self.unbounded.remove(best_v)

            logger.info(f"Current partial ordering: {self.order}")

        self.order.append(self.unbounded.pop())



        return VariableOrdering(self.order[::-1], history, ["variables", "entropy"])

    
    def __compute_V(self, var1, var2 = None):
        def get_V(cm):
            n = np.sum(cm)
            X2, p, dof, _ = chi2_contingency(cm)
            phi = X2 / n
            V = np.sqrt(phi / (min(cm.shape) - 1))
            nr, nc = cm.shape 

            phi_bar = max(0, phi - (nr-1)*(nc-1)/(n-1))
            shapes_ = [(x - (((x-1)**2) / (n-1))) for x in cm.shape]
            V_bar = np.sqrt(phi_bar / (min(shapes_) - 1))

            return V, V_bar, (X2, p, dof)

        second_arg = self.df[var2] if var2 is not None else \
            self.df[self.order].apply(lambda seq: tuple(seq), axis=1) 
        contingency_matrix = pd.crosstab(self.df[var1], second_arg).to_numpy()
        return get_V(contingency_matrix)


    def __chi2_heu(self):
        df_header = ["var_1", "var_2", "V", "X2", "p-value", "dof", "critical_value"]
        prob = 0.99
        best_V, best_var = 0, None 
        tests = list() 

        #select first pair of variable of the ordering
        for v1, v2 in combinations(self.variables, 2):
            V, V_corr, (X2, p, dof) = self.__compute_V(v1, v2)
            critical = chi2.ppf(prob, dof)
            if abs(X2) >= critical and V_corr >= best_V:
                best_V, best_var = V_corr, (v1, v2)
            tests.append([v1, v2, V_corr, X2, p, dof, critical])

        print(f"Best starting variables: {best_var}")

        self.init_order()
        #remove chosen variables from unbounded ones and add them to the current partial ordering
        self.unbounded = self.unbounded.difference(best_var)
        self.order.extend(best_var)

        while len(self.unbounded) > 1:
            best_V, best_var = -1, None 
            for var in self.unbounded:
                V, V_corr, (X2, p, dof) = self.__compute_V(var)
                critical = chi2.ppf(prob, dof)

                #if abs(X2) >= critical and V_corr > best_V:
                if V_corr > best_V:
                    best_V, best_var = V_corr, var 
                
                tests.append([stringify_ordering(self.order), var, V_corr, X2, p, dof, critical])

            if best_var is None:
                # print(best)
                raise Exception("Unexpected situation here :v")

            self.order.append(best_var)
            self.unbounded.remove(best_var)
        
        self.order.append(self.unbounded.pop())

        return VariableOrdering(self.order, tests, df_header)



def reset_terminals(inname, outname):
    df = pd.read_csv(inname, sep="\t")
    columns = "\t".join(df.columns.tolist() )
    domain_row = "\t".join(df.iloc[0].dropna().astype(int).astype(str))

    df = df.iloc[1:].astype(int)
    df["terminal"] = 1 

    with open(outname, "w") as f:
        f.write(f"{columns}\n{domain_row}\n")
        df.to_csv(f, header=False, index=False, sep="\t")


def add_df_row(dataframe, row):
    dataframe.loc[-1] = row 
    dataframe.index = dataframe.index + 1 
    return dataframe.sort_index()

def stringify_ordering(variables):
    return ",".join(map(lambda v: v.replace("v_", ""), variables))

if __name__ == "__main__":
    parser = argparse.ArgumentParser() 
    parser.add_argument("--data", type=str, nargs="+", required=True)
    parser.add_argument("--vofile", type=str, default="variable_orderings.tsv")

    args = parser.parse_args() 

    df = pd.DataFrame(columns=["path", "input_file", "db_name", "ordering", "heuristic"])

    for filename in args.data:
        folder = os.path.dirname(os.path.abspath(filename))
        basename = os.path.basename(filename)
        name = os.path.splitext(basename)[0].replace("_all_paths", "")

        print(f"Current file: {name}")

        heu = Heu(filename)

        #heu.compute_heuristic("entropy_counts")

        #raise Exception()

        for h in heu.available_heuristics:
            print(f"Applying {h} heuristic:", flush=True)
            try:
                ordering = heu.compute_heuristic(h)
                df = add_df_row(df, [folder, basename, name, stringify_ordering(ordering.ordering), h])
                print(f"Ordering found: {ordering.ordering}")
           #     print(f"Current stats:\n{ordering.stats}", flush=True)
            except:
                print(f"Unable to calculate {h} for file {filename}", flush=True)

        # ordering = heu.sort_by_entropy() 
        # ordering_s = stringify_ordering(ordering)

        # data = [folder, basename, name, ordering_s, "entropy"]
        # df = add_df_row(df, data)

        # print(f"Ordering: {ordering_s}")
        # print(f"{ordering}")

        # #maybe useless, maybe not-let's try... - if the node_id is not in last position, let's put it 
        # if  False and ordering[-1] != heu.col_node_id:
        #     ordering2 = heu.sort_by_entropy(consider_node_id=False)

        #     if ordering != ordering2:
        #         #add ordering that ends with node_id variable 
        #         ordering_s = stringify_ordering(ordering2)
        #         data = [folder, basename, name, ordering_s, "entropy_wo"]

        #         df = add_df_row(df, data)

        # notermfile = f"{name}_all_paths_no_occ.dd_txt"
        # reset_terminals(filename, os.path.join(folder, notermfile))
    else:
        vo_output_filename = os.path.join(folder, args.vofile)
        df.to_csv(vo_output_filename, sep="\t", index=False, header=False)
