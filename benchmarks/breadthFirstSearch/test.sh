#!/bin/bash
set -e

NAMES=("simpleBFS" "ndBFS" "serialBFS" "backForwardBFS" "deterministicBFS")

ROOT_DIR=/afs/ece/project/seth_group/ziqiliu
PRR_DIR=$ROOT_DIR/static-prr
INSTRUMENT_DIR=$ROOT_DIR/instrument-test
CHEETAH_DIR=$ROOT_DIR/cheetah/build/

for TESTNAME in "${NAMES[@]}"
do 
    pushd $TESTNAME
    clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -ftapir=serial -flto -fuse-ld=lld -O3 \
        --opencilk-resource-dir=$CHEETAH_DIR \
        -Wall -O3 -std=c++17 \
        --gcc-toolchain=$ROOT_DIR/GCC-12.2.0 \
        -S -emit-llvm -o BFSTime.ll -c ../bench/BFSTime.C

    clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -ftapir=serial -flto -fuse-ld=lld -O3 \
        --opencilk-resource-dir=$CHEETAH_DIR \
        -Wall -O3 -std=c++17 \
        --gcc-toolchain=$ROOT_DIR/GCC-12.2.0  \
        -c BFS.C -S -emit-llvm -o BFS.ll

    llvm-link -S BFS.ll BFSTime.ll -o BFSLink.ll

    opt -load $PRR_DIR/libPrrStatistic.so -load-pass-plugin $PRR_DIR/libPrrStatistic.so -passes="prr-stats" -test=$TESTNAME BFSLink.ll --disable-output
    # opt --enable-new-pm=0 -S --prr -test=${TESTNAME} BFSLink.ll -o BFSLink-pr.ll
    # run pbbsv2-dbg to run static functon prstate on parallel_for calls specifically 
    opt -load $PRR_DIR/libParallelForDebugInfo.so -load-pass-plugin $PRR_DIR/libParallelForDebugInfo.so -passes="pbbsv2-dbg" -test=$TESTNAME BFSLink.ll --disable-output

    echo "$TESTNAME done!"
    popd
done