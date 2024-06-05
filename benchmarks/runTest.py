import os
import pandas as pd
import subprocess
import time
import argparse
import zipfile

# helper function that cleans up sp_name
def cleanPForSPName(sp_name):
    sp_name = str(sp_name)
    prefix = "parallel_for<(lambda at "
    if (sp_name[:len(prefix)] != prefix):
        return sp_name # remove quotations around sp_name
    pfor_part = sp_name[len(prefix):len(sp_name)-2] # ignore ">)" at the end
    (filename, ln, col) = pfor_part.split(':') 
    # clean up filename
    if (filename[:len("../bench/common/../")] == "../bench/common/../"):
        filename = "./" + filename[len("../bench/common/../"):]
    elif filename[:len("./parlay/internal/../internal/delayed/")] == "./parlay/internal/../internal/delayed/":
        filename = "./parlay/internal/delayed/" + filename[len("./parlay/internal/../internal/delayed/"):]
    
    return "parallel_for: " + ":".join([filename, ln, col])

def prettyPrint(df, outpath=None): 

    def format_column(column, width):
        return column.apply(lambda x: '{:<{}}'.format(x, width))

    with pd.option_context('display.max_colwidth', 1000):
        # Calculate the maximum width needed for each column
        max_widths = df.apply(lambda col: col.astype(str).map(len).max())

        # Format each column
        formatted_columns = [format_column(df[col].astype(str), width) for col, width in max_widths.iteritems()]

        # Reconstruct the DataFrame with formatted columns
        formatted_df = pd.concat(formatted_columns, axis=1)

        # Convert to string
        formatted_string = formatted_df.to_string(index=False, header=True)
        
        if outpath is None: 
            print(formatted_string)    
        else: 
            with open(outpath, 'w') as f: 
                f.write(formatted_string) 

def runPbbsV2StaticTest(exclude_dirs, args):
    
    # run all test.sh in pbbs_v2 directory
    test_scripts = []   
    for dirpath, dirnames, filenames in os.walk('.'):
        dirnames[:] = [ d for d in dirnames if d not in exclude_dirs]
        if "test.sh" in filenames:
            test_scripts.append((dirpath, dirnames, filenames))

    failed_test = []
    orig_dir = os.getcwd()
    
    # # DEBUG
    # os.chdir(r"./comparisonSort")
    # subprocess.call(["./test.sh"])
    # os.chdir(orig_dir)  
    # #######

    for dirpath, dirnames, filenames in test_scripts:
        os.chdir(dirpath)

        print("\n\nRunning {}...".format(os.path.join(dirpath, 'test.sh')))
        if (subprocess.call(["./test.sh"]) != 0):
            # failed_test.append((dirpath, dirnames, filenames))
            print("{} failed!".format(os.path.join(dirpath, 'test.sh')))
            # return
        os.chdir(orig_dir)

    # post-process all json files in pbbs_v2 directory
    json_files = []
    for dirpath, dirnames, filenames in os.walk("."):
        dirnames[:] = [ d for d in dirnames if d not in exclude_dirs ]
        for file in filenames:
            if file.endswith('.json'):
                full_path = os.path.join(dirpath, file)
                # print(full_path)
                json_files.append(full_path)
    # print(json_files)

    data_file = []
    for file in json_files:
        with open(file, 'r') as f: 
            # Load JSON data into a Series
            data_series = pd.read_json(f, typ='series')

            # Convert the Series to a DataFrame
            data_df = pd.DataFrame([data_series])

            # Add the filename as a new column
            data_df.insert(0, 'filename', file[:-5])

            # Append this DataFrame to the list
            data_file.append(data_df)

    df = pd.concat(data_file, ignore_index=True, sort=True)

    # print(df)

    # write df as .csv
    df.to_csv("pbbs_v2.csv", index=False)

def runInstrumentation(exclude_dirs): 
    # run all runner.sh scripts
    pbbs_dir = os.getcwd( )
    fail_tests = []
    for test_dir, dirs, files in os.walk('.'):
        dirs[:] = [ d for d in dirs if d not in exclude_dirs ]
        for file in files:
            if file == "runner.sh":
                os.chdir(test_dir)
                subprocess.call(["./runner.sh"])
                os.chdir(pbbs_dir)

def runPbbsV2RuntimeCheck(exclude_dirs, args):
    orig_dir = os.getcwd()

    # static parallel_for func prr state: looking for .dbg.csv
    # for each entry in .dbg.csv that has state "defef" or "defdac"
    # if state "defef": check name not in .dac.csv
    # if state "defdac": check name not in .ef.csv
    # else collect cases
    pbbsv2ToDfdbg = dict()
    pbbsv2ToDfDAC = dict()
    pbbsv2ToDfEF = dict()
    for root, dirs, files in os.walk("."):
        dirs[:] = [ d for d in dirs if d not in exclude_dirs ]
        for file in files: 
            if file.endswith(".dac.csv"):
                # print("dac: " + os.path.join(root, file))
                dfDAC = pd.read_csv(os.path.join(root, file))
                # clean sp_name:
                dfDAC['sp_name'] = dfDAC['sp_name'].apply(cleanPForSPName)
                
                # with pd.option_context('display.max_colwidth', 1000):
                #     print(dfDAC.head(10).to_string(index=False))
                pbbsv2ToDfDAC[root] = dfDAC
            elif file.endswith(".ef.csv"):
                # print("ef: " + os.path.join(root, file))
                dfEF = pd.read_csv(os.path.join(root, file))
                # clean sp_name: 
                dfEF['sp_name'] = dfEF['sp_name'].apply(cleanPForSPName)
                # with pd.option_context('display.max_colwidth', 1000):
                #     print(dfEF.head(10).to_string(index=False))
                pbbsv2ToDfEF[root] = dfEF
            elif file.endswith(".dbg.csv"):
                dfDbg = pd.read_csv(os.path.join(root, file))
                # clean sp_name: 
                dfDbg['sp_name'] = dfDbg['sp_name'].apply(cleanPForSPName)
                pbbsv2ToDfdbg[root] = dfDbg
    # collect failed cases: (test_name : { sp_name + , + linkage_name : reason })
    # "reason" can be one of the following: 
    # 1) <sp_name + linkage_name> appears multiple times in .dbg.csv
    # 2) <sp_name + linkage_name> has static prr state 'defdac' but have non-zero 'defef' entry count in dynamic runtime check
    # 3) <sp_name + linkage_name> has static prr state 'defef' but have non-zero 'defdac' entry count in dynamic runtime check 
    
    # failed correctness case
    failed_case = pd.DataFrame(columns=['pbbsv2-test-name', 'sp_name', 'linkage_name', 'reason'])
    # conservative: for any static indefinite pfor, check if the analysis is too strict
    conservative_case = pd.DataFrame(columns=['pbbsv2-test-name', 'sp_name', 'linkage_name', 'reason'])

    for pbbsv2Test, dfDbg in pbbsv2ToDfdbg.items():
        if pbbsv2Test not in pbbsv2ToDfDAC or pbbsv2Test not in pbbsv2ToDfEF:
            continue
        # 
        dfDef = dfDbg[(dfDbg['prs']=="defef") | (dfDbg['prs']=="defdac")]
        #
        dfBoth = dfDbg[(dfDbg['prs'] == "both")]
        #     
        dfDAC = pbbsv2ToDfDAC[pbbsv2Test]
        #
        dfEF = pbbsv2ToDfEF[pbbsv2Test]
        # correctness: for any static definite pfor, verify their dynamic entry count
        for _, row in dfDef.iterrows():
            sp_name = row['sp_name']
            linkage_name = row['linkage_name']
            if row['prs'] == "defef":
                cond = (dfDAC['sp_name'] == sp_name) & (dfDAC['pfor_name'] == linkage_name)

                if dfDAC[cond].shape[0] > 0:
                    # static claimed def has dac entry counts
                    reason = "has static prr state 'defef' but have non-zero 'defdac' entry count in dynamic runtime check"
                    fail = pd.Series([pbbsv2Test, sp_name, linkage_name, reason], index=failed_case.columns)
                    failed_case = failed_case.append(fail, ignore_index=True) 
                        
            elif row['prs'] == "defdac":
                cond = (dfEF['sp_name'] == sp_name) & (dfEF['pfor_name'] == linkage_name)
                if dfEF[cond].shape[0] > 0:
                    # statically claimed dac has def entry counts   
                    reason = "has static prr state 'defdac' but have non-zero 'defef' entry count in dynamic runtime check"
                    fail = pd.Series([pbbsv2Test, sp_name, linkage_name, reason], index=failed_case.columns)
                    failed_case = failed_case.append(fail, ignore_index=True)
            elif dfDef[(dfDef['sp_name'] == sp_name) & (dfDef['linkage_name'] == linkage_name)].shape[0] > 1:
                reason = "appears multiple times in .dbg.csv"
                fail = pd.Series([pbbsv2Test, sp_name, linkage_name, reason], index=failed_case.columns)
                failed_case = failed_case.append(fail, ignore_index=True)
        # conservative check: 
        for _, row in dfBoth.iterrows():
            sp_name = row['sp_name']
            linkage_name = row['linkage_name']
            dacCond = (dfDAC['sp_name'] == sp_name) & (dfDAC['pfor_name'] == linkage_name)
            efCond = (dfEF['sp_name'] == sp_name) & (dfEF['pfor_name'] == linkage_name)
            if dfDAC[dacCond].shape[0] == 0 and dfEF[efCond].shape[0] == 0:
                # print("  skipped!")
                continue # ignore pfor that were not dynamically executed
            if dfDAC[dacCond].shape[0] > 0 and dfEF[efCond].shape[0] == 0:
                reason = "statically both pfor has only dac entry"
                conservative = pd.Series([pbbsv2Test, sp_name, linkage_name, reason], index=conservative_case.columns)
                conservative_case = conservative_case.append(conservative, ignore_index=True)
            elif dfDAC[dacCond].shape[0] == 0 and dfEF[efCond].shape[0] > 0:
                reason = "statically both pfor has only ef entry"
                conservative = pd.Series([pbbsv2Test, sp_name, linkage_name, reason], index=conservative_case.columns)
                conservative_case = conservative_case.append(conservative, ignore_index=True)

    ##### Visualize result ################
    # visualize failed_case
    failed_case = failed_case.sort_values(by="linkage_name")
    print("\nPrinting failed case...")
    print("There are in total " + str(failed_case.shape[0]) + " incorrect pfors")
    prettyPrint(failed_case)
    # with pd.option_context('display.max_colwidth', 1000):
    #     # Calculate the maximum width needed for each column
    #     max_widths = failed_case.apply(lambda col: col.astype(str).map(len).max())

    #     # Format each column
    #     formatted_columns = [format_column(failed_case[col].astype(str), width) for col, width in max_widths.iteritems()]

    #     # Reconstruct the DataFrame with formatted columns
    #     formatted_df = pd.concat(formatted_columns, axis=1)

    #     # Convert to string
    #     formatted_string = formatted_df.to_string(index=False, header=True)
    #     print(formatted_string)

    # visualize over-conservative case
    conservative_case = conservative_case.sort_values(by="linkage_name")
    print("\nPrinting overconservative case...")
    print("There are in total " + str(conservative_case.shape[0]) + " overconservative pfors")
    prettyPrint(conservative_case)
    # with pd.option_context('display.max_colwidth', 1000):
    #     # Calculate the maximum width needed for each column
    #     max_widths = conservative_case.apply(lambda col: col.astype(str).map(len).max())

    #     # Format each column
    #     formatted_columns = [format_column(conservative_case[col].astype(str), width) for col, width in max_widths.iteritems()]

    #     # Reconstruct the DataFrame with formatted columns
    #     formatted_df = pd.concat(formatted_columns, axis=1)

    #     # Convert to string
    #     formatted_string = formatted_df.to_string(index=False, header=True)
    #     print(formatted_string)

    # name list of all pbbsv2 test run
    print("\nTested...")
    for pbbsv2test in sorted(pbbsv2ToDfdbg.keys()):
        print(pbbsv2test)
    
    return 

def runPforinst(exclude_dirs):
    output_files = []
    for test_dir, dirs, files in os.walk('.'):
        dirs[:] = [ d for d in dirs if d not in exclude_dirs ]
        for file in files:
            if file.endswith(".pforinst.csv"):
                # with open(os.path.join(test_dir, file), 'r') as f:
                #     print("\n\n" + '=' * 10 + "\n")
                #     print(file + "....")
                #     df = pd.read_csv(f)
                #     # drop duplicate rows
                #     df["FileAndLineNumber"] = df["FileAndLineNumber"].apply(lambda x: x.lstrip())
                #     df.drop_duplicates(inplace=True)
                #     df.reset_index(drop=True, inplace=True)

                #     # print 
                #     prettyPrint(df, outpath=os.path.join(test_dir, file[:-len(".csv")] + ".txt"))
                output_files.append(os.path.join(test_dir, file))

    with zipfile.ZipFile("pforinst.zip", 'w', zipfile.ZIP_DEFLATED, allowZip64=True) as zipf:
        for pforinst_path in output_files:
            zipf.write(pforinst_path, arcname=os.path.basename(pforinst_path))

def collectAllStatistics(exclude_dirs):
    """
    compile all runtime and static results into one file for easier querying 
    """
    # comparisonSort = ['mergeSort']
    ###### static ##########
    columns=['pbbs_name', 'mangled_name', 'dismangled_name', 'stat_state']
    df_static_ls = []
    for root, dirs, files in os.walk("."):
        dirs[:] = [ d for d in dirs if d not in exclude_dirs ]

        for file in files: 
            pbbs_name = root # os.path.basename(root)
            if (file.endswith(".dbg.csv")):
                # .dbg.csv
                df_dbg_csv = pd.read_csv(os.path.join(root, file))
                df_dbg_csv.drop('temp_file', axis=1, inplace=True)
                df_dbg_csv.rename(
                    columns={
                        'prs':'stat_state',
                        'sp_name':'dismangled_name',
                        'linkage_name':'mangled_name'
                    }, inplace=True)
                df_dbg_csv['pbbs_name'] = pbbs_name
                df_static_ls.append(df_dbg_csv)

    df_static = pd.concat(df_static_ls, ignore_index=True)

    ###### runtime #########
    # .dac.csv: sum of times function has dac entries
    # columns=['pbbs_name', 'mangled_name', 'dismangled_name', 'rt_dac_cnt']
    df_dac_ls = [] 
    for root, dirs, files in os.walk("."):
        dirs[:] = [ d for d in dirs if d not in exclude_dirs ]
        for file in files:
            pbbs_name = root # os.path.basename(root)
            if (file.endswith(".dac.csv")):
                df_dac_csv = pd.read_csv(os.path.join(root, file))
                df_dac_csv.rename(
                    columns={   
                        'dac_count':'rt_dac_cnt',
                        'sp_name':'dismangled_name',
                        'pfor_name':'mangled_name'
                    }, inplace=True)
                df_dac_csv['pbbs_name'] = pbbs_name
                df_dac_ls.append(df_dac_csv)

    df_dac = pd.concat(df_dac_ls, ignore_index=True)
                
    # .ef.csv: sum of times function has ef entries
    # columns=['pbbs_name', 'mangled_name', 'dismangled_name', 'rt_ef_cnt']
    df_ef_ls = []
    for root, dirs, files in os.walk("."):
        dirs[:] = [ d for d in dirs if d not in exclude_dirs ]
        for file in files:
            pbbs_name = root # os.path.basename(root)
            if (file.endswith(".ef.csv")):
                df_ef_csv = pd.read_csv(os.path.join(root, file))
                df_ef_csv.rename(
                    columns={   
                        'ef_count':'rt_ef_cnt', 'sp_name':'dismangled_name', 'pfor_name':'mangled_name'
                    }, inplace=True)
                df_ef_csv['pbbs_name'] = pbbs_name
                df_ef_ls.append(df_ef_csv) 
    df_ef = pd.concat(df_ef_ls, ignore_index=True)
    
    ######## result file ##########
    # merge df_dac and df_ef: pbbs_name, mangled_name, dismangled_name, rt_ef_cnt, rt_dac_cnt
    df_rt_entry = pd.merge(df_dac, df_ef, on=['pbbs_name', 'mangled_name', 'dismangled_name'], how="outer")
    #  fill in n/a with 0
    df_rt_entry.fillna(value={'rt_ef_cnt':0.0, 'rt_dac_cnt':0.0}, inplace=True)

    # merge df_rt_entry with df_static: pbbs_name, mangled_name, dismangled_name, stat_state, rt_ef_cnt, rt_dac_cnt
    df_rt_entry_good = df_rt_entry[df_rt_entry['mangled_name'].isin(df_static['mangled_name'])]
    df_rt_entry_bad  = df_rt_entry[~df_rt_entry['mangled_name'].isin(df_static['mangled_name'])]
    df_rt_entry_static_states = pd.merge(df_rt_entry_good, df_static, on=['pbbs_name', 'mangled_name', 'dismangled_name'], how="outer")
    # fill in n/a with 0.0
    df_rt_entry_static_states.fillna(value={'rt_ef_cnt':0.0, 'rt_dac_cnt':0.0}, inplace=True)

    print("df_rt_entry that is not in df_static")
    print(df_rt_entry_bad[['pbbs_name', 'dismangled_name', 'mangled_name', 'rt_ef_cnt', 'rt_dac_cnt']].head(80).to_string())
    print(df_rt_entry_bad.shape)
    
    # output to file
    output_path = r"/afs/ece/project/seth_group/ziqiliu/test/pbbs_v2/everything.csv"
    df_rt_entry_static_states = df_rt_entry_static_states[['pbbs_name', 'dismangled_name', 'mangled_name', 'stat_state', 'rt_ef_cnt', 'rt_dac_cnt']]
    df_rt_entry_static_states.to_csv(output_path, index=False)
        
def run():
    
    """
    parser flag definitions 
    """
    parser = argparse.ArgumentParser()
    parser.add_argument("--static", help="run prr-stats and pbbsv2-dbg pass on pbbsv2 benchmarks", action="store_true")
    parser.add_argument("--runner", help="run dynamic instrumentation on pbbsv2 benchmark", action="store_true")
    parser.add_argument("--check", help="run correctness check between static analysis result and dynamic instrumentation on pbbsv2 benchmark", action="store_true")
    parser.add_argument("--pforinst", help="collect builtin pforinst data on pbbsv2 benchmark", action="store_true")
    parser.add_argument("--collect", help="collect and merge all runtime and static data into one csv file", action="store_true")
    
    args = parser.parse_args()

    # program execution shared data
    exclude_dirs = ["./venv", "./ANN", "./common"]

    # run ./test.sh in each pbbsv2 
    if args.static:
        runPbbsV2StaticTest(exclude_dirs, args)

    # run pforinst and correctness checking instrumentation
    if args.runner:
        runInstrumentation(exclude_dirs)

    # check static results with runtime instrumentation: compare .dbg.csv with .ef.csv and .dac.csv
    if args.check: 
        runPbbsV2RuntimeCheck(exclude_dirs, args)

    # 
    if args.pforinst:
        runPforinst(exclude_dirs)

    # 
    if args.collect:
        collectAllStatistics(exclude_dirs)


if __name__ == "__main__":
    run()