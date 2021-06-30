#!/usr/bin/env python3 

# fa un file excel che summarize i report degli ordinamenti con e senza conteggi 

import argparse, os 
import pandas as pd

if __name__ == "__main__":
    parser = argparse.ArgumentParser() 
    parser.add_argument("-i", "--input_folder", type=str, required=True)
    parser.add_argument("-o", "--output_file", type=str, required=True)
    args = parser.parse_args()

    input_folder = args.input_folder
    output_filename = args.output_file if args.output_file.endswith(".xlsx") else f"{args.output_file}.xlsx"

    targets = ("report_full.tsv", "report_wocounts.tsv", "variable_orderings.tsv")
    full_report_filename, wocounts_report_filename, variable_orderings_filename = targets

    reports = dict() 
    orderings = dict() 

    for root, _, files in os.walk(input_folder):
        if full_report_filename in files and variable_orderings_filename in files:
            orderings[root] = pd.read_csv(
                os.path.join(root, variable_orderings_filename), 
                sep="\t", 
                names=["a","filename","name","ordering","method"])

            filename = os.path.join(root, full_report_filename)
            fullreport_df = pd.read_csv(filename, sep="\t")
            fullreport_df["counts"] = True

            filename = os.path.join(root, wocounts_report_filename)
            if os.path.exists(filename):
                wocountsreport_df = pd.read_csv(os.path.join(root, wocounts_report_filename), sep="\t")
                wocountsreport_df["counts"] = False 

                reports[root] = pd.concat([fullreport_df, wocountsreport_df], ignore_index=True) 
            else:
                reports[root] = fullreport_df
            
            reports[root].sort_values(by=["name", "counts", "memory_used"], ascending=(True, True, True), inplace=True)
                
            
    with pd.ExcelWriter(output_filename, engine="openpyxl") as excelfile:
        for folder, report in reports.items():
            sheet = folder.replace("/", " ").replace(".", " ").strip().replace(" ", "_")
            report.to_excel(excelfile, sheet_name=sheet, index=False)

        #XXX FORMAT NUMBERS 
        workbook = excelfile.book 
#        format_numbers = workbook.add_format() #workbook.add_format({"num_format": "#,##0"})

  #      for worksheet in excelfile.sheets:
            
 #           worksheet.set_column(1, 6, cell_format = format_numbers)