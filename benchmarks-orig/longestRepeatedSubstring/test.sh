#!/bin/bash
set -e
ROOT_DIR=/afs/ece/project/seth_group/ziqiliu
NAMES=("doubling" "sequential-suffix-array" "sequential-suffix-tree")
CHEETAH_DIR=$ROOT_DIR/cheetah/build/
PRR_DIR=$ROOT_DIR/static-prr

for TESTNAME in "${NAMES[@]}"
do 
    pushd ${TESTNAME}
    # echo "$TESTNAME..."
    clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -ftapir=serial -flto -fuse-ld=lld -O3 \
        --opencilk-resource-dir=$CHEETAH_DIR \
        -Wall -O3 -std=c++17 \
        --gcc-toolchain=$ROOT_DIR/GCC-12.2.0 \
        -S -emit-llvm -o lrsTime.ll -c ../bench/lrsTime.C

    clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -ftapir=serial -flto -fuse-ld=lld -O3 \
        --opencilk-resource-dir=$CHEETAH_DIR \
        -Wall -O3 -std=c++17 \
        --gcc-toolchain=$ROOT_DIR/GCC-12.2.0  \
        -c lrs.C -S -emit-llvm -o lrs.ll

    llvm-link -S lrs.ll lrsTime.ll -o lrsLink.ll

    # opt --enable-new-pm=0 -S --prr -test=${TESTNAME} lrsLink.ll -o lrsLink-pr.ll
    opt -load $PRR_DIR/libPrrStatistic.so -load-pass-plugin $PRR_DIR/libPrrStatistic.so -passes="prr-stats" -test=${TESTNAME} lrsLink.ll --disable-output
    # run pbbsv2-dbg to run static functon prstate on parallel_for calls specifically 
    opt -load $PRR_DIR/libParallelForDebugInfo.so -load-pass-plugin $PRR_DIR/libParallelForDebugInfo.so -passes="pbbsv2-dbg" -test=$TESTNAME lrsLink.ll --disable-output

    # # generate callgraph dot 
    # opt -enable-new-pm=0 -dot-callgraph -callgraph-heat-colors lrsLink.ll > /dev/null

    # # generate callgraph png
    # dot -Tpng lrsLink.ll.callgraph.dot -o lrsLink.png

    # print done
    echo "$TESTNAME done!" 
    popd
done