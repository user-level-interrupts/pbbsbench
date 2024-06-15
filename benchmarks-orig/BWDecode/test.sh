#!/bin/bash
ROOT_DIR=/afs/ece/project/seth_group/ziqiliu

PRR_DIR=$ROOT_DIR/static-prr
INSTRUMENT_DIR=$ROOT_DIR/instrument-test
CHEETAH_DIR=$ROOT_DIR/cheetah/build/

TEST_DIR=./listRank

pushd $TEST_DIR
clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
    -ftapir=serial -flto -fuse-ld=lld -O3 \
    --opencilk-resource-dir=$CHEETAH_DIR \
    -Wall -O3 -std=c++17 \
    --gcc-toolchain=$ROOT_DIR/GCC-12.2.0 \
    -S -emit-llvm -o bwTime.ll -c ../bench/bwTime.C

clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
    -ftapir=serial -flto -fuse-ld=lld -O3 \
    --opencilk-resource-dir=$CHEETAH_DIR \
    -Wall -O3 -std=c++17 \
    --gcc-toolchain=$ROOT_DIR/GCC-12.2.0 \
    -c bw.C -S -emit-llvm -o bw.ll

llvm-link -S bw.ll bwTime.ll -o bwLink.ll

# opt --enable-new-pm=0 -S --prr -test=bw bwLink.ll -o bwLink-pr.ll
opt -load $PRR_DIR/libPrrStatistic.so -load-pass-plugin $PRR_DIR/libPrrStatistic.so -passes="prr-stats" -test=bw bwLink.ll --disable-output

# run pbbsv2-dbg to run static functon prstate on parallel_for calls specifically 
opt -load $PRR_DIR/libParallelForDebugInfo.so -load-pass-plugin $PRR_DIR/libParallelForDebugInfo.so -passes="pbbsv2-dbg" -test=bw bwLink.ll --disable-output

popd