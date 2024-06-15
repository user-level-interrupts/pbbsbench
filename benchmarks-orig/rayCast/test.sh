#!/bin/bash
set -e
ROOT_DIR=/afs/ece/project/seth_group/ziqiliu
PRR_DIR=$ROOT_DIR/static-prr
INSTRUMENT_DIR=/afs/ece/project/seth_group/ziqiliu/instrument-test
CHEETAH_DIR=/afs/ece/project/seth_group/ziqiliu/cheetah/build/

TESTNAME="kdTree"
pushd $TESTNAME
clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
    -ftapir=serial -flto -fuse-ld=lld -O3 \
    --opencilk-resource-dir=$CHEETAH_DIR \
    -Wall -O3 -std=c++17 \
    --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0 \
    -S -emit-llvm -o rayTime.ll -c ../bench/rayTime.C

clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
    -ftapir=serial -flto -fuse-ld=lld -O3 \
    --opencilk-resource-dir=$CHEETAH_DIR \
    -Wall -O3 -std=c++17 \
    --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0 \
    -S -emit-llvm -c ray.C -o ray.ll

llvm-link -S ray.ll rayTime.ll -o rayLink.ll

# opt --enable-new-pm=0 -S --prr -test=${TESTNAME} rayLink.ll -o rayLink-pr.ll
opt -load $PRR_DIR/libPrrStatistic.so -load-pass-plugin $PRR_DIR/libPrrStatistic.so -passes="prr-stats" -test=${TESTNAME} rayLink.ll --disable-output
# run pbbsv2-dbg to run static functon prstate on parallel_for calls specifically 
opt -load $PRR_DIR/libParallelForDebugInfo.so -load-pass-plugin $PRR_DIR/libParallelForDebugInfo.so -passes="pbbsv2-dbg" -test=$TESTNAME rayLink.ll --disable-output

echo "$TESTNAME done!"
popd