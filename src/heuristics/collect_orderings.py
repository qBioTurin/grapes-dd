#!/usr/bin/env python3 

import argparse
from logging import warning
import os, sys
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from numpy import histogram
import pandas as pd 
import re 
import enum 
import warnings
from collections import defaultdict



class datatype(enum.Enum):
    ERDOS = "Erdos"
    BARABASI = "Barabasi"
    FORESTFIRE = "ForestFire"
    CUSTOM = "Biological"

    @classmethod
    def from_string(cls, s):
        for x in cls:
            if x.value.lower() == s.lower():
                return x 
        return cls.CUSTOM

class columns(enum.Enum):
    LP = "lp"
    GRAPH_NAME = "graph_db_name"
    NUM_NODES_GRAPH = "g_num_nodes"
    NUM_LABELS_GRAPH = "g_num_labels"
    PEAK_NODES = "peak_nodes"
    PEAK_MEMORY = "peak_memory"
    MEMORY_USED = "memory_used"
    ORDERING = "ordering"


class dataset:
    def __init__(self, name: str, data: pd.DataFrame, typeofdata: datatype) -> None:
        self.name = name 
        self._df = data 
        self.type = typeofdata

        self._custom_cols = list()
        self.__num_nodes = None
        self.__num_labels = None
        self.__plabels = None 
    
    @property
    def num_nodes(self):
        return self.__num_nodes
    
    @property 
    def num_labels(self):
        return self.__num_labels

    @property
    def label_percentage(self):
        return self.__plabels
    
    @property
    def dataframe(self):
        return self._df

    @num_nodes.setter
    def num_nodes(self, nn):
        self.__num_nodes = int(nn)

    @label_percentage.setter 
    def label_percentage(self, p):
        self.__plabels = float(p) / 100
        if not (0 <= self.__plabels <= 1):
            warnings.warn("Warning - label probability is not in [0,1]")
        if not self.num_nodes:
            warnings.warn("Warning - num nodes = 0")
        self.__num_labels = self.num_nodes * self.__plabels 
    

    def enrich_dataframe(self):
        nnodes, nlabels = "g_num_nodes", "g_num_labels"
        
        if nnodes not in self.dataframe.columns and self.num_nodes and self.num_labels:
            self._custom_cols.extend([nnodes, nlabels])
            self._df[nnodes] = self.num_nodes 
            self._df[nlabels] = self.num_labels

            self._df[nnodes].astype(int)
            self._df[nlabels].astype(int)

        return self 
    
    def _reindex_df(self):
        new_cols = set(self._custom_cols)
        next_cols = [x for x in self.dataframe.columns if x not in new_cols]
        new_ordering = self._custom_cols + next_cols
        self._df = self._df.reindex(columns=new_ordering)


    

class barabasi(dataset):
    def __init__(self, folder, name, data) -> None:
        super().__init__(None, data, datatype.BARABASI)
        #e.g. folder:   test/barabasi/20000/2
        self.num_nodes, degree = folder.split("/")[-2:]
        self.degree = int(degree)
        #e.g. name:     target-power1.5-1_0.1
        power, plabels = name.split("-")[-2:]
        self.alpha = float(power.replace("power", ""))
        self.label_percentage = float(plabels.split("_")[-1]) #percentage

        #bb_nnodes_nlabels_alpha_degree
        self.name = f"bb_{self.num_nodes}_{self.num_labels}_{self.alpha}_{self.degree}"
    
    def enrich_dataframe(self):
        super().enrich_dataframe()
        alpha, avg_degree = "alpha", "g_avg_degree"
        if alpha not in self.dataframe.columns:
            self._df[alpha] = self.alpha
            self._df[avg_degree] = self.degree
            self._df[avg_degree].astype(int)

            self._custom_cols.extend([alpha, avg_degree])
            self._reindex_df()

        return self 



class forestfire(dataset):
    def __init__(self, folder, name, data) -> None:
        super().__init__(None, data, datatype.FORESTFIRE)
        #e.g. folder:   test/forestfire/10000/0.7
        self.num_nodes, p = folder.split("/")[-2:]
        self.prob = float(p)
        #e.g. name:     target1_10
        self.label_percentage = float(name.split("_")[-1])  #percentage

        #ff_nnodes_nlabels_p
        self.name = f"ff_{self.num_nodes}_{self.num_labels}_{self.prob}"
    

    def enrich_dataframe(self):
        super().enrich_dataframe()
        p_col = "p"
        if p_col not in self.dataframe.columns:
            self._df[p_col] = self.prob  

            self._custom_cols.append(p_col)
            self._reindex_df() 
        return self 


class biological(dataset):
    def __init__(self, net_name: str, data: pd.DataFrame) -> None:
        super().__init__(net_name, data, datatype.CUSTOM)

    def enrich_dataframe(self):
        super().enrich_dataframe() 
        db_name_col = columns.GRAPH_NAME.value
        if db_name_col not in self.dataframe.columns:
            self._df[db_name_col] = self.name
            self._custom_cols.append(db_name_col)
            self._reindex_df()
        return self 

class resultssss:
    def __init__(self) -> None:
        self.data_collection = defaultdict(list)
        self.df_collection = dict()
        self.__regex = re.compile(r'\.lp_(\d+)\.tsv$') #match the extension of results file: .lp_L.tsv


    def collect(self, input_folder):
        #load data from files, then merge dataframes belonging to graphs of the same type
        return self.__load(input_folder).__init() 
    
    def load(self, input_file):
        with pd.ExcelFile(input_file, engine="openpyxl") as ew:
            aggregated_sheets = filter(lambda name: "_L" not in name and name != "histogram", ew.sheet_names)
            self.df_collection = {
                datatype.from_string(sheet): pd.read_excel(ew, sheet_name=sheet) 
                for sheet in aggregated_sheets}
            

    def search(self, queries: pd.DataFrame, name_col, ordering_col, method_col):
        cols = [name_col, ordering_col, method_col]
        df = self.df_collection[datatype.CUSTOM].copy(deep=True)

        if columns.GRAPH_NAME.value not in df.columns:
            df.columns = [columns.GRAPH_NAME.value if i == 0 else x for i, x in enumerate(df.columns)]

        if columns.LP.value not in df.columns:
            df[columns.LP.value] = [len(y) - 1 for y in df[columns.ORDERING.value].str.split(",")]

        unnamed_cols = list(filter(lambda s: s.lower().startswith("unnamed:"), df.columns))
        if unnamed_cols:
            df.drop(columns = unnamed_cols, inplace = True)

        lp_values, db_names = set(), set()

        query_tuples = queries.itertuples(index=False)

        for row in query_tuples:
            name, ordering, heu = [row[c] for c in cols]
            lp = len(ordering.split(",")) - 1
            try:
                df.loc[(df[columns.LP.value] == lp) & (df[columns.GRAPH_NAME.value] == name) & (df[columns.ORDERING.value] == ordering), "heuristic"] = heu
            except:
                raise Exception("Some expected column is not present :v -- in search method")
            
            lp_values.add(lp)
            db_names.add(name)

        return df.loc[(df[columns.GRAPH_NAME.value].isin(db_names)) & df[columns.LP.value].isin(lp_values)]

    def __init(self): 
        for graph_type, related_results in self.data_collection.items():
            specific_columns = related_results[0]._custom_cols
            self.df_collection[graph_type] = pd.concat([x.dataframe for x in related_results], axis=0)\
                .sort_values(by=[*specific_columns, columns.MEMORY_USED.value])

        return self 

    def __load(self, input_folder):
        def load_df(filename) -> pd.DataFrame:
            cols = set(pd.read_csv(filename, sep="\t", nrows =1))
            wanted = cols.difference({"peak_nodes", "peak_memory"})
            return pd.read_csv(filename, sep="\t", usecols=wanted)

        for root, _, files in os.walk(input_folder):
            for f in files:
                if (match := re.search(self.__regex, f)):
                    df = load_df(os.path.join(root, f)).sort_values(by=[columns.MEMORY_USED.value])
                    df[columns.LP.value] = lp = int(match.group(1))
                    net_name = f.replace(f".lp_{lp}.tsv", "")

                    obj, params = biological, dict(net_name = net_name, data = df)

                    if (baraby := ("barabasi" in root)) or "forestfire" in root:
                        obj = barabasi if baraby else forestfire
                        params = dict(folder = root, data = df, name = net_name)
                    
                    self.add(obj(**params).enrich_dataframe())
        
        return self 
    

    def write(self, outfile):
        excel_extension = ".xlsx"
        output_file = outfile if outfile.endswith(excel_extension) else f"{outfile.strip()}{excel_extension}"

        with pd.ExcelWriter(output_file) as ew:
            for typeofdataset, related_results in self.data_collection.items():
                specific_columns = related_results[0]._custom_cols
                df = pd.concat([x.dataframe for x in related_results], axis=0).sort_values(
                    by=[*specific_columns, columns.MEMORY_USED.value])
                
                #save all data of the current type
                sheet_name = typeofdataset.value

                df.to_excel(ew, sheet_name=sheet_name, index=False)

                #save results grouped by LP 
                lp_col = df[columns.LP.value]
                for lp in sorted(pd.unique(lp_col)):
                    df[lp_col == lp].to_excel(ew, sheet_name=f"{sheet_name}_L{lp}", index=False)
        

            histograms = defaultdict(lambda: defaultdict(list))
            for typeofdataset, related_results in self.data_collection.items():            
                for x in related_results:
                    best = x.dataframe.iloc[0]
                    lp = len(best.ordering.split(",")) - 1
                    histograms[lp][best.ordering].append(x.name)

            df_data = [
                [lp, ordering, len(occurrences), ",".join(occurrences)] 
                    for lp, hist in histograms.items()
                        for ordering, occurrences in hist.items() ]

            pd.DataFrame(df_data, columns=["lp", "ordering", "occurrences", "which"])\
                .sort_values(by=["lp", "occurrences"], ascending=(True, False))\
                .to_excel(ew, sheet_name="histogram", index=False)
                


    def add(self, d: dataset):
        self.data_collection[d.type].append(d)


def subplotta(dflist, df_names, lp, colormap):
    my_df = [df.copy()  
        for (name, curr_lp), df in dflist.items() 
            if curr_lp == lp and name in df_names]
    
    nrows, ncols = 2, len(my_df) // 2 
    fig, axes = plt.subplots(nrows, ncols)
    # plt.figure(dpi=dpi)

    index = 0 
    for r in range(nrows):
        for c in range(ncols):
            df, ax = my_df[index], axes[r, c]
            # (df-df.min())/(df.max()-df.min())
            # target = df.loc[columns.MEMORY_USED.value]
            # print(target)
            t = df[columns.MEMORY_USED.value]
            df[columns.MEMORY_USED.value] = (t - t.min()) / (t.max() - t.min())

            df.plot(x="ranking", y = columns.MEMORY_USED.value, kind="scatter", c = "#000000", ax=ax)

            heu_df = df.dropna(subset=["heuristic"])
            heu_df.plot(x="ranking", y = columns.MEMORY_USED.value, kind="scatter", c="#FF0000", ax=ax)

            for i in range(len(heu_df)):
                ranking, mem, h = heu_df.iloc[i][["ranking", columns.MEMORY_USED.value, "heuristic"]]
                ax.plot(ranking, mem, "s", color=colormap[h])

            ax.title.set_text(heu_df.iloc[0][columns.GRAPH_NAME.value])
            ax.grid(True)
            # ax.xlabel("rank")
            # ax.ylabel("memory p")
            index += 1


    #build legend
    handles, labels = ax.get_legend_handles_labels()
    for h, color in colormap.items():
        handles.append( mpatches.Patch(color=color, label=h))

    plt.legend(handles = handles, bbox_to_anchor=(0.6, 0), loc="lower right", 
                bbox_transform=fig.transFigure, ncol=3)
    plt.xlabel("rank")
    plt.ylabel("memory")

    plt.show() 



if __name__ == "__main__":
    parser = argparse.ArgumentParser() 
    parser.add_argument("-i", "--input_folder", type=str)
    parser.add_argument("-o", "--output_file", type=str)

    parser.add_argument("-x", "--xtable", type=str)
    parser.add_argument("-q", "--query", type=str, nargs="*")
    args = parser.parse_args()


    if args.input_folder and (not os.path.exists(args.input_folder) or not os.path.isdir(args.input_folder)):
        print(f"Input folder {args.input_folder} can't be found")
        sys.exit(1)

    results = resultssss()
    
    if args.input_folder:
        results.collect(args.input_folder)
        results.write(args.output_file)
    elif args.xtable:
        results.load(args.xtable)
    else:
        raise Exception()


    if args.query:
        dflist = list()

        for queryfile in args.query:
            if any([queryfile.endswith(ext) for ext in (".csv", ".tsv")]):
                separator = "\t" if queryfile.endswith(".tsv") else ","
                dflist.append(pd.read_csv(queryfile, sep=separator, header=None))
            else:
                print(f"Skipped file {queryfile}: unsupported file format!")

        query_df = pd.concat(dflist)
        # founds = results.search(query_df, name_col=0, method_col=7, ordering_col=8)
        # print(founds)

        founds = results.search(query_df, name_col=0, ordering_col=8, method_col=7)
        if not founds.empty:
            outfile = "prova.xlsx"
            outfile2 = "prova2.xlsx"
            sheets_to_plot = dict()

            with pd.ExcelWriter(outfile) as ew, pd.ExcelWriter(outfile2) as ew2:
                for name, df in founds.groupby(by=[columns.GRAPH_NAME.value]):
                    for lp, spec_df in df.groupby(by=[columns.LP.value]):
                        curr = spec_df.sort_values(by=[columns.MEMORY_USED.value])
                        curr["ranking"] = range(1, 1 + len(curr))

                        if lp >= 3:
                            sheets_to_plot[(name, lp)] = curr.copy() 

                        # spec_df.sort_values(by=[columns.MEMORY_USED.value]).to_excel(
                        curr.to_excel(ew, sheet_name=f"{name}_lp_{lp}", index=False)
                        
                        if pd.isna(curr.iloc[0]["heuristic"]):
                            curr.loc[curr["ranking"] == 1, "heuristic"] = "best"
                        if pd.isna(curr.iloc[-1]["heuristic"]):
                            curr.loc[curr["ranking"] == len(curr), "heuristic"] = "worst"

                        curr = curr.dropna(subset=["heuristic"])
                        curr.to_excel(ew2, sheet_name=f"{name}_lp_{lp}", index=False)   

            colors = ["tab:orange", "tab:green", "tab:red", "tab:olive", "tab:cyan"]
            available_h = ["entropy", "correlation"]
            available_h.extend([f"{h}_reverse" for h in available_h] + ["default"])
            colormap = {h: c for h, c in zip(available_h, colors)}


            plot_main = set(["AIDO99SD.40000", "cmap_all.200", "db.600", "ppigo"])
            plot_ppi = set(["ppigo", "CE", "MUS", "DROSOFILA", "HOMO", "YEAST"])


            print(sheets_to_plot.keys())

            subplotta(sheets_to_plot, plot_main, 4, colormap)
            subplotta(sheets_to_plot, plot_ppi, 4, colormap)

            subplotta(sheets_to_plot, plot_main, 3, colormap)
            subplotta(sheets_to_plot, plot_ppi, 3, colormap)

            
            
            # raise Exception()

            # for (name, lp), df in sheets_to_plot.items():
            #     fig, ax = plt.subplots()
            #     handles, labels = ax.get_legend_handles_labels()

            #     df.plot(x="ranking", y = columns.MEMORY_USED.value, kind="scatter", ax=ax)

            #     heu_df = df.dropna(subset=["heuristic"])
            #     heu_df.plot(x="ranking", y = columns.MEMORY_USED.value, kind="scatter", c="#FF0000", ax=ax)

            #     for i in range(len(heu_df)):
            #         ranking, mem, h = heu_df.iloc[i][["ranking", columns.MEMORY_USED.value, "heuristic"]]
            #         plt.plot(ranking, mem, "s", color=colormap[h])

            #         handles.append( mpatches.Patch(color=colormap[h], label=h))

            #     plt.legend(handles = handles, loc="lower right")
            #     plt.ylabel("Memory used (B)")
            #     plt.title(f"Memory used plot for {name} lp={lp}")
            #     plt.grid(True)
            #     plt.savefig(os.path.join(args.output_file, f"{name}_lp_{lp}.png"), dpi=300)


    