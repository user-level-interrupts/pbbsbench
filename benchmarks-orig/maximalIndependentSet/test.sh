#!/bin/bash
set -e

ROOT_DIR=/afs/ece/project/seth_group/ziqiliu
PRR_DIR=$ROOT_DIR/static-prr
INSTRUMENT_DIR=$ROOT_DIR/instrument-test
CHEETAH_DIR=$ROOT_DIR/cheetah/build/

NAMES=("incrementalMIS" "ndMIS" "serialMIS")

for TESTNAME in "${NAMES[@]}"
do 
    pushd $TESTNAME
    clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -ftapir=serial -flto -fuse-ld=lld -O3 \
        --opencilk-resource-dir=$CHEETAH_DIR \
        -Wall -O3 -std=c++17 \
        --gcc-toolchain=$ROOT_DIR/GCC-12.2.0 \
        -S -emit-llvm -o MISTime.ll -c ../bench/MISTime.C

    clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -ftapir=serial -flto -fuse-ld=lld -O3 \
        --opencilk-resource-dir=$CHEETAH_DIR \
        -Wall -O3 -std=c++17 \
        --gcc-toolchain=$ROOT_DIR/GCC-12.2.0  \
        -c MIS.C -S -emit-llvm -o MIS.ll

    llvm-link -S MIS.ll MISTime.ll -o MISLink.ll

    # opt --enable-new-pm=0 -S --prr -test=${TESTNAME} MISLink.ll -o MISLink-pr.ll
    opt -load $PRR_DIR/libPrrStatistic.so -load-pass-plugin $PRR_DIR/libPrrStatistic.so -passes="prr-stats" -test=${TESTNAME} MISLink.ll --disable-output
    # run pbbsv2-dbg to run static functon prstate on parallel_for calls specifically 
    opt -load $PRR_DIR/libParallelForDebugInfo.so -load-pass-plugin $PRR_DIR/libParallelForDebugInfo.so -passes="pbbsv2-dbg" -test=$TESTNAME MISLink.ll --disable-output

    echo "$TESTNAME done!"
    popd
done
