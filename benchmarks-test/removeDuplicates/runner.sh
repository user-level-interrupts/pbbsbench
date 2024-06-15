#!/bin/bash

ROOT_DIR=/afs/ece/project/seth_group/ziqiliu
PRR_DIR=$ROOT_DIR/static-prr
INSTRUMENT_DIR=$ROOT_DIR/instrument-test
CHEETAH_DIR=$ROOT_DIR/cheetah/build/

NAMES=("parlayhash" "serial_hash" "serial_sort")
for TESTNAME in "${NAMES[@]}"
do
    pushd $TESTNAME
    clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -fforkd=lazy -ftapir=serial \
        --opencilk-resource-dir=../../../../cheetah/build/ \
        --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0 \
        -mcx16 -O3 -std=c++17 -DNDEBUG -I . \
        -Xclang -fpass-plugin=$INSTRUMENT_DIR/instrument-pass.so \
        -include dedup.h -o dedup.o -c ../bench/dedupTime.C

    clang++ -gdwarf-2 -mcx16 -O3 -std=c++17 \
        -fforkd=lazy -ftapir=serial -ldl -fuse-ld=lld \
        -L/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64 \
        -L${INSTRUMENT_DIR} -lInstrument \
        dedup.o -o dedup
    
    CILK_NWORKERS=1 LD_LIBRARY_PATH=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64:/afs/ece/project/seth_group/ziqiliu/instrument-test \
        ./dedup -r 2 -i /afs/ece/project/seth_group/pakha/pbbsbench/testData/sequenceData/data/randomSeq_100M_int

    mv ef.csv $TESTNAME.ef.csv
    mv dac.csv $TESTNAME.dac.csv

    #### run pforinst instrumentation #######
    clang++ -gdwarf-2 -DBUILTIN -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -fforkd=lazy -ftapir=serial \
        --opencilk-resource-dir=../../../../cheetah/build/ \
        --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0 \
        -mcx16 -O3 -std=c++17 -DNDEBUG -I . \
        -Xclang -fpass-plugin=$INSTRUMENT_DIR/libBuiltinIntrinsic.so \
        -include dedup.h -o dedup.o -c ../bench/dedupTime.C

    clang++ -gdwarf-2 -mcx16 -O3 -std=c++17 \
        -fforkd=lazy -ftapir=serial -ldl -fuse-ld=lld \
        -L/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64 \
        -L${INSTRUMENT_DIR} -lPforinst \
        dedup.o -o dedup
    
    CILK_NWORKERS=1 LD_LIBRARY_PATH=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64:/afs/ece/project/seth_group/ziqiliu/instrument-test \
        ./dedup -r 2 -i /afs/ece/project/seth_group/pakha/pbbsbench/testData/sequenceData/data/randomSeq_100M_int

    mv pforinst.csv $TESTNAME.pforinst.csv

    echo "$TESTNAME done!"
    popd
done