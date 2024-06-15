#!/bin/bash
set -e

ROOT_DIR=/afs/ece/project/seth_group/ziqiliu
PRR_DIR=$ROOT_DIR/static-prr

INSTRUMENT_DIR=$ROOT_DIR/instrument-test
CHEETAH_DIR=$ROOT_DIR/cheetah/build/

NAMES=("parallel" "sequential")
for TESTNAME in "${NAMES[@]}"
do 
    pushd $TESTNAME

    clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -ftapir=serial -flto -fuse-ld=lld -O3 \
        --opencilk-resource-dir=$CHEETAH_DIR \
        -Wall -O3 -std=c++17 \
        --gcc-toolchain=$ROOT_DIR/GCC-12.2.0 \
        -S -emit-llvm -o histogramTime.ll -c ../bench/histogramTime.C

    clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -ftapir=serial -flto -fuse-ld=lld -O3 \
        --opencilk-resource-dir=$CHEETAH_DIR \
        -Wall -O3 -std=c++17 \
        --gcc-toolchain=$ROOT_DIR/GCC-12.2.0 \
        -S -emit-llvm -c histogram.C -o histogram.ll

    llvm-link -S histogram.ll histogramTime.ll -o histogramLink.ll

    # opt --enable-new-pm=0 -S --prr -test=${TESTNAME} histogramLink.ll -o histogramLink-pr.ll
    opt -load $PRR_DIR/libPrrStatistic.so -load-pass-plugin $PRR_DIR/libPrrStatistic.so -passes="prr-stats" -test=${TESTNAME} histogramLink.ll --disable-output
    # run pbbsv2-dbg to run static functon prstate on parallel_for calls specifically 
    opt -load $PRR_DIR/libParallelForDebugInfo.so -load-pass-plugin $PRR_DIR/libParallelForDebugInfo.so -passes="pbbsv2-dbg" -test=$TESTNAME histogramLink.ll --disable-output

    echo "$TESTNAME done" 
    popd
done