#!/bin/bash

INSTRUMENT_DIR=/afs/ece/project/seth_group/ziqiliu/instrument-test
CHEETAH_DIR=/afs/ece/project/seth_group/ziqiliu/cheetah/build/

clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
    -fforkd=lazy -ftapir=serial \
    --opencilk-resource-dir=$CHEETAH_DIR \
    --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0 \
    -mcx16 -O3 -std=c++17 -DNDEBUG -I . -ldl -fuse-ld=lld \
    -Xclang -fpass-plugin=$INSTRUMENT_DIR/libBuiltinIntrinsic.so \
    -o MISTime.o -c ./bench/MISTime.C \
    -L${INSTRUMENT_DIR} -lPforinst

NAMES=("incrementalMIS" "ndMIS" "serialMIS")

for TESTNAME in "${NAMES[@]}"
do 
    echo "$TESTNAME..."
    pushd $TESTNAME

    clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -fforkd=lazy -ftapir=serial \
        --opencilk-resource-dir=$CHEETAH_DIR \
        --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0 \
        -mcx16 -O3 -std=c++17 -DNDEBUG -I . -ldl -fuse-ld=lld \
        -Xclang -fpass-plugin=$INSTRUMENT_DIR/libBuiltinIntrinsic.so \
        -c MIS.C -o MIS.o \
        -L${INSTRUMENT_DIR} -lPforinst

    clang++ -o MIS ../MISTime.o MIS.o \
        -gdwarf-2 -DPARLAY_OPENCILK -fforkd=lazy -ftapir=serial -ldl -fuse-ld=lld \
        -L/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64 \
        -L${INSTRUMENT_DIR} -lPforinst

    CILK_NWORKERS=1 LD_LIBRARY_PATH=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64:/afs/ece/project/seth_group/ziqiliu/instrument-test \
        ./MIS -r 2 -i /afs/ece/project/seth_group/pakha/pbbsbench/testData/graphData/data/rMatGraph_JR_12_16000000

    mv pforinst.csv $TESTNAME.pforinst.csv
    popd
done