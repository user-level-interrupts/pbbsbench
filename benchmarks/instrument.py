import os
import pandas as pd
import subprocess
import argparse

def runLowering(exclude_dirs): 
    # execute builtin.sh in each pbbsv2 test directory
    pbbs_dir = os.getcwd( )
    fail_tests = []
    for test_dir, dirs, files in os.walk('.'):
        dirs[:] = [ d for d in dirs if d not in exclude_dirs ]
        for file in files:
            if file == "builtin.sh":
                os.chdir(test_dir)
                if subprocess.call(["./builtin.sh"]) != 0:
                    fail_test.append(test_dir)
                os.chdir(pbbs_dir)
    
    # print failed tests
    print("The following test did not successfully lower __builtin_uli_lazyd_inst")
    for fail_dir in fail_tests: 
        print(fail_dir)
    # 
    return

def interpret_results(exclude_dirs):

    for test_dir, dirs, files in os.walk('.'):
        dirs[:] = [ d for d in dirs if d not in exclude_dirs ]
        for file in files:
            if file.endswith(".pforinst.csv"):
                with open(os.path.join(test_dir, file), 'r') as f:
                    print(file + "....")
                    df = pd.read_csv(f)
                    # drop duplicate rows
                    df["FileAndLineNumber"] = df["FileAndLineNumber"].apply(lambda x: x.lstrip())
                    df.drop_duplicates(inplace=True)
                    df.reset_index(drop=True, inplace=True)

                    # write to csv 
                    df.to_csv(file, index=False)

                    # with pd.option_context('display.max_colwidth', 1000):
                    #     print(df.to_string())
    
def run():
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-rerun", help="don't rerun ./builtin.sh", action="store_true")
    args = parser.parse_args()
    # parser.add_argument("--builtin-uli-lazyd", help="lowering pass that replace __builtin_uli_lazyd_inst calls with call to lazydIntrumentLoop", action="store_true")

    # run lowering pass
    exclude_dirs = ["./venv", "./pANN", "./common"]
    if not args.no_rerun:
        runLowering(exclude_dirs)

    interpret_results(exclude_dirs)

if __name__ == "__main__":
    run()