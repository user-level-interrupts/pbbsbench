#!/bin/bash
set -e

INSTRUMENT_DIR=/afs/ece/project/seth_group/ziqiliu/instrument-test
CHEETAH_DIR=/afs/ece/project/seth_group/ziqiliu/cheetah/build/

clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
    -fforkd=lazy -ftapir=serial \
    --opencilk-resource-dir=$CHEETAH_DIR \
    --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0 \
    -mcx16 -O3 -std=c++17 -DNDEBUG -I . -ldl -fuse-ld=lld \
    -Xclang -fpass-plugin=$INSTRUMENT_DIR/instrument-pass.so \
    -o MSTTime.o -c ./bench/MSTTime.C \
    -L${INSTRUMENT_DIR} -lInstrument

clang++ -gdwarf-2 -DBUILTIN -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
    -fforkd=lazy -ftapir=serial \
    --opencilk-resource-dir=$CHEETAH_DIR \
    --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0 \
    -mcx16 -O3 -std=c++17 -DNDEBUG -I . -ldl -fuse-ld=lld \
    -Xclang -fpass-plugin=$INSTRUMENT_DIR/libBuiltinIntrinsic.so \
    -o MSTTime-pforinst.o -c ./bench/MSTTime.C \
    -L${INSTRUMENT_DIR} -lPforinst
    
NAMES=("parallelFilterKruskal" "serialMST" "parallelKruskal")
for TESTNAME in "${NAMES[@]}"
do 
    pushd $TESTNAME
    
    clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -fforkd=lazy -ftapir=serial \
        --opencilk-resource-dir=$CHEETAH_DIR \
        --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0 \
        -mcx16 -O3 -std=c++17 -DNDEBUG -I . -ldl -fuse-ld=lld \
        -Xclang -fpass-plugin=$INSTRUMENT_DIR/instrument-pass.so \
        -c MST.C -o MST.o \
        -L${INSTRUMENT_DIR} -lInstrument

    clang++ -o MST ../MSTTime.o MST.o \
        -gdwarf-2 -DPARLAY_OPENCILK -fforkd=lazy -ftapir=serial -ldl -fuse-ld=lld \
        -L/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64 \
        -L${INSTRUMENT_DIR} -lInstrument
    
    CILK_NWORKERS=1 LD_LIBRARY_PATH=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64:/afs/ece/project/seth_group/ziqiliu/instrument-test \
        ./MST -r 2 -i /afs/ece/project/seth_group/pakha/pbbsbench/testData/graphData/data/rMatGraph_WE_12_2250000

    mv ef.csv $TESTNAME.ef.csv
    mv dac.csv $TESTNAME.dac.csv
    mv caller.csv $TESTNAME.caller.csv

    ######### run pforinst instrumentation ###########
    clang++ -gdwarf-2 -DBUILTIN -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -fforkd=lazy -ftapir=serial \
        --opencilk-resource-dir=$CHEETAH_DIR \
        --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0 \
        -mcx16 -O3 -std=c++17 -DNDEBUG -I . -ldl -fuse-ld=lld \
        -Xclang -fpass-plugin=$INSTRUMENT_DIR/libBuiltinIntrinsic.so \
        -c MST.C -o MST.o \
        -L${INSTRUMENT_DIR} -lPforinst

    clang++ -o MST ../MSTTime-pforinst.o MST.o \
        -gdwarf-2 -DBUILTIN -DPARLAY_OPENCILK -fforkd=lazy -ftapir=serial -ldl -fuse-ld=lld \
        -L/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64 \
        -L${INSTRUMENT_DIR} -lPforinst
    
    CILK_NWORKERS=1 LD_LIBRARY_PATH=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64:/afs/ece/project/seth_group/ziqiliu/instrument-test \
        ./MST -r 2 -i /afs/ece/project/seth_group/pakha/pbbsbench/testData/graphData/data/rMatGraph_WE_12_2250000

    mv pforinst.csv $TESTNAME.pforinst.csv

    echo "$TESTNAME done!"
    popd
done