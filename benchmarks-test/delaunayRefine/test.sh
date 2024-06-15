#!/bin/bash
set -e


ROOT_DIR=/afs/ece/project/seth_group/ziqiliu
PRR_DIR=$ROOT_DIR/static-prr
CHEETAH_DIR=$ROOT_DIR/cheetah/build/

TESTNAME="incrementalRefine"

pushd $TESTNAME
clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
    -ftapir=serial -flto -fuse-ld=lld -O3 \
    --opencilk-resource-dir=$CHEETAH_DIR \
    -Wall -O3 -std=c++17 \
    --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0 \
    -S -emit-llvm -o refineTime.ll -c ../bench/refineTime.C

clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
    -ftapir=serial -flto -fuse-ld=lld -O3 \
    --opencilk-resource-dir=$CHEETAH_DIR \
    -Wall -O3 -std=c++17 \
    --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0 \
    -S -emit-llvm -c refine.C -o refine.ll

llvm-link -S refine.ll refineTime.ll -o refineLink.ll

# opt --enable-new-pm=0 -S --prr -test=${TESTNAME} refineLink.ll -o refineLink-pr.ll
opt -load $PRR_DIR/libPrrStatistic.so -load-pass-plugin $PRR_DIR/libPrrStatistic.so -passes="prr-stats" -test=${TESTNAME} refineLink.ll --disable-output

# run pbbsv2-dbg to run static functon prstate on parallel_for calls specifically 
opt -load $PRR_DIR/libParallelForDebugInfo.so -load-pass-plugin $PRR_DIR/libParallelForDebugInfo.so -passes="pbbsv2-dbg" -test=$TESTNAME refineLink.ll --disable-output

echo "$TESTNAME done!"
popd