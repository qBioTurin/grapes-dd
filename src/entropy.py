#!/usr/bin/env python3 

import argparse
import logging
import numpy as np 
import pandas as pd 
from scipy.stats import entropy 
from collections import Counter


# Enable logging
logging.basicConfig(
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s', level=logging.INFO
)
logger = logging.getLogger(__name__)


class Ordify:
    def __init__(self, strings, var_names = None):
        if isinstance(strings, list):
            data = [list(s) for s in strings]
            self.variables = [f"v{n+1}" for n in range(len(data[0]))] if not var_names else var_names
            self.df = pd.DataFrame(data, columns = self.variables)
        elif isinstance(strings, pd.DataFrame):
            self.df = strings.astype(str) 
            self.variables = list(self.df.columns )

        self.order = list()
        self.unbounded = self.variables.copy() 


    def combine(self):
        while len(self.unbounded) > 1:
            h = list() 

            #combine each unbounded variable with partial ordered variables
            for variable in self.unbounded:
                vars = self.order + [variable]
                #create kmers combining variables belonging to the partial order and current variable
                df = self.df[vars].apply(lambda seq: "".join(seq), axis=1)
                #count occurrences and estimate entropy for current set of objects 
                kmers, n_occ = np.unique(df, return_counts=True)
        #        print(list(zip(kmers, n_occ)))
                h.append(entropy(n_occ, base=2))

                logger.info(f"Entropy for {vars}: {h[-1]}")
            #add variable with minimum entropy to the order 
            series = pd.Series(h, index = self.unbounded)
            best_var = series.idxmin()

            self.order.append(best_var)
            self.unbounded.remove(best_var)

            logger.info(f"Current partial ordering: {self.order}")

        self.order.append(self.unbounded[0])
        self.unbounded.pop()





if __name__ == "__main__":
    parser = argparse.ArgumentParser() 
    parser.add_argument("--data", type=str, required=True)
    args = parser.parse_args() 

    df = pd.read_csv(args.data, sep="\t")
    df = df.iloc[1:].drop(columns=["terminal"])

    logger.info(f"Loading data completed. Data shape: {df.shape}")

    col_id = max([int(col.replace("v_", "")) for col in df.columns if col.startswith("v_")])

    df.drop(columns=[f"v_{col_id}"], inplace=True)

    ord = Ordify(df)
    ord.combine()

    best_order = [v.replace("v_", "") for v in ord.order]
    if "terminal" in best_order:
        best_order.remove("terminal ")
    best_order.append(str(col_id))

    logger.info(f"Order found: {best_order}")

    print(",".join(best_order))

#    print(f"Best order: {ord.order}")

 #   print(ord.df[ord.order].sort_values(by=ord.order))