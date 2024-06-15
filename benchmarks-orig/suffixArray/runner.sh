#!/bin/bash
set -e
ROOT_DIR=/afs/ece/project/seth_group/ziqiliu
PRR_DIR=$ROOT_DIR/static-prr
INSTRUMENT_DIR=/afs/ece/project/seth_group/ziqiliu/instrument-test
CHEETAH_DIR=/afs/ece/project/seth_group/ziqiliu/cheetah/build/

clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
    -fforkd=lazy -ftapir=serial \
    --opencilk-resource-dir=$CHEETAH_DIR \
    --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0  \
    -mcx16 -O3 -std=c++17 -DNDEBUG -I . \
    -Xclang -fpass-plugin=$INSTRUMENT_DIR/instrument-pass.so \
    -o SATime.o -c ./bench/SATime.C \
    -L${INSTRUMENT_DIR} -lInstrument

NAMES=("parallelKS" "parallelRange" "serialDivsufsort")
for TESTNAME in "${NAMES[@]}"
do 
    pushd $TESTNAME

    clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -fforkd=lazy -ftapir=serial \
        --opencilk-resource-dir=$CHEETAH_DIR \
        --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0  \
        -mcx16 -O3 -std=c++17 -DNDEBUG -I . \
        -Xclang -fpass-plugin=$INSTRUMENT_DIR/instrument-pass.so \
        -c SA.C -o SA.o \
        -L${INSTRUMENT_DIR} -lInstrument

    clang++ -o SA ../SATime.o SA.o \
        -gdwarf-2 -DPARLAY_OPENCILK -fforkd=lazy -ftapir=serial -ldl -fuse-ld=lld \
        -L/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64 \
        -L${INSTRUMENT_DIR} -lInstrument

    CILK_NWORKERS=1 LD_LIBRARY_PATH=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64:/afs/ece/project/seth_group/ziqiliu/instrument-test \
        ./SA -r 2 -i /afs/ece/project/seth_group/pakha/pbbsbench/testData/sequenceData/data/trigramString_100000000
    
    mv ef.csv $TESTNAME.ef.csv
    mv dac.csv $TESTNAME.dac.csv

    echo "$TESTNAME done!"
    popd
done