#!/bin/bash
# set -e

INSTRUMENT_DIR=/afs/ece/project/seth_group/ziqiliu/instrument-test
CHEETAH_DIR=/afs/ece/project/seth_group/ziqiliu/cheetah/build/
PRR_DIR=/afs/ece/project/seth_group/ziqiliu/static-prr

NAMES=("mergeSort" "sampleSort" "quickSort" "serialSort" "stableSampleSort" "stlParallelSort")

for TESTNAME in "${NAMES[@]}"
do
    pushd $TESTNAME

    clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -ftapir=serial -flto -fuse-ld=lld -O3 \
        --opencilk-resource-dir=$CHEETAH_DIR \
        -Wall -O3 -std=c++17 \
        --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0 \
        -S -emit-llvm -include sort.h -o sort.ll -c ../bench/sortTime.C \
        -ldl -L/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64

    # opt --enable-new-pm=0 -S --prr -test=${TESTNAME} sort.ll -o sort-pr.ll
    opt -load $PRR_DIR/libPrrStatistic.so -load-pass-plugin $PRR_DIR/libPrrStatistic.so -passes="prr-stats" -test=$TESTNAME sort.ll --disable-output
    
    # run pbbsv2-dbg to run static functon prstate on parallel_for calls specifically 
    opt -load $PRR_DIR/libParallelForDebugInfo.so -load-pass-plugin $PRR_DIR/libParallelForDebugInfo.so -passes="pbbsv2-dbg" -test=$TESTNAME sort.ll --disable-output

    # opt -load $PRR_DIR/libCallgraphExplain.so -load-pass-plugin $PRR_DIR/libCallgraphExplain.so -passes="callgraph-explain" -test=$TESTNAME sort.ll --disable-output > scc_out.txt
    echo "$TESTNAME done!"
    
    popd
done