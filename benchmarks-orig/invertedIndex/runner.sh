#!/bin/bash
set -e

ROOT_DIR=/afs/ece/project/seth_group/ziqiliu
PRR_DIR=$ROOT_DIR/static-prr
INSTRUMENT_DIR=$ROOT_DIR/instrument-test
CHEETAH_DIR=$ROOT_DIR/cheetah/build/

clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
    -fforkd=lazy -ftapir=serial \
    --opencilk-resource-dir=$CHEETAH_DIR \
    --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0  \
    -mcx16 -O3 -std=c++17 -DNDEBUG -I . \
    -Xclang -fpass-plugin=$INSTRUMENT_DIR/instrument-pass.so \
    -o indexTime.o -c ./bench/indexTime.C \
    -L${INSTRUMENT_DIR} -lInstrument

clang++ -gdwarf-2 -DBUILTIN -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
    -fforkd=lazy -ftapir=serial \
    --opencilk-resource-dir=$CHEETAH_DIR \
    --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0  \
    -mcx16 -O3 -std=c++17 -DNDEBUG -I . \
    -Xclang -fpass-plugin=$INSTRUMENT_DIR/libBuiltinIntrinsic.so \
    -o indexTime-pforinst.o -c ./bench/indexTime.C \
    -L${INSTRUMENT_DIR} -lPforinst

NAMES=("parallel" "sequential")

for TESTNAME in "${NAMES[@]}"
do 
    pushd $TESTNAME

    clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -fforkd=lazy -ftapir=serial \
        --opencilk-resource-dir=$CHEETAH_DIR \
        --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0  \
        -mcx16 -O3 -std=c++17 -DNDEBUG -I . \
        -Xclang -fpass-plugin=$INSTRUMENT_DIR/instrument-pass.so \
        -c index.C -o index.o \
        -L${INSTRUMENT_DIR} -lInstrument

    clang++ -o index ../indexTime.o index.o \
        -gdwarf-2 -DPARLAY_OPENCILK -fforkd=lazy -ftapir=serial -ldl -fuse-ld=lld \
        -L/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64 \
        -L${INSTRUMENT_DIR} -lInstrument

    CILK_NWORKERS=1 LD_LIBRARY_PATH=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64:/afs/ece/project/seth_group/ziqiliu/instrument-test \
        ./index -r 2 -i /afs/ece/project/seth_group/pakha/pbbsbench/testData/sequenceData/data/wikisamp.xml
    
    mv ef.csv $TESTNAME.ef.csv
    mv dac.csv $TESTNAME.dac.csv

    #### run pforinst instrumentaion 
    clang++ -gdwarf-2 -DBUILTIN -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -fforkd=lazy -ftapir=serial \
        --opencilk-resource-dir=$CHEETAH_DIR \
        --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0  \
        -mcx16 -O3 -std=c++17 -DNDEBUG -I . \
        -Xclang -fpass-plugin=$INSTRUMENT_DIR/libBuiltinIntrinsic.so \
        -c index.C -o index.o \
        -L${INSTRUMENT_DIR} -lPforinst

    clang++ -o index ../indexTime-pforinst.o index.o \
        -gdwarf-2 -DBUILTIN -DPARLAY_OPENCILK -fforkd=lazy -ftapir=serial -ldl -fuse-ld=lld \
        -L/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64 \
        -L${INSTRUMENT_DIR} -lPforinst

    CILK_NWORKERS=1 LD_LIBRARY_PATH=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64:/afs/ece/project/seth_group/ziqiliu/instrument-test \
        ./index -r 2 -i /afs/ece/project/seth_group/pakha/pbbsbench/testData/sequenceData/data/wikisamp.xml
    
    mv pforinst.csv $TESTNAME.pforinst.csv

    echo "$TESTNAME done!"
    popd
done