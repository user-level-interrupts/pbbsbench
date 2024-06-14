#!/bin/bash
# set -e
ROOT_DIR=/afs/ece/project/seth_group/ziqiliu
INSTRUMENT_DIR=/afs/ece/project/seth_group/ziqiliu/instrument-test
CHEETAH_DIR=/afs/ece/project/seth_group/ziqiliu/cheetah/build/

NAMES=("mergeSort" "sampleSort" "quickSort" "serialSort" "stableSampleSort" "stlParallelSort")
for TESTNAME in "${NAMES[@]}"
do
    pushd $TESTNAME
    
    clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -fforkd=lazy -ftapir=serial \
        --opencilk-resource-dir=../../../../cheetah/build/ \
        --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0 \
        -mcx16 -O3 -std=c++17 -DNDEBUG -I . \
        -Xclang -fpass-plugin=$INSTRUMENT_DIR/instrument-pass.so \
        -include sort.h -c ../bench/sortTime.C -o sort.o

    clang++ -gdwarf-2 -mcx16 -O3 -std=c++17 \
        -fforkd=lazy -ftapir=serial -ldl -fuse-ld=lld \
        -L/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64 \
        -L${INSTRUMENT_DIR} -lInstrument \
        sort.o -o sort

    CILK_NWORKERS=1 LD_LIBRARY_PATH=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64:/afs/ece/project/seth_group/ziqiliu/instrument-test \
        ./sort -r 2 -i /afs/ece/project/seth_group/pakha/pbbsbench/testData/sequenceData/data/randomSeq_100M_double

    mv ef.csv $TESTNAME.ef.csv
    mv dac.csv $TESTNAME.dac.csv

    #### run pforinst instrumentation
    clang++ -gdwarf-2 -DBUILTIN -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -fforkd=lazy -ftapir=serial \
        --opencilk-resource-dir=../../../../cheetah/build/ \
        --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0 \
        -mcx16 -O3 -std=c++17 -DNDEBUG -I . \
        -Xclang -fpass-plugin=$INSTRUMENT_DIR/libBuiltinIntrinsic.so \
        -include sort.h -c ../bench/sortTime.C -o sort.o

    clang++ -gdwarf-2 -mcx16 -O3 -std=c++17 \
        -fforkd=lazy -ftapir=serial -ldl -fuse-ld=lld \
        -L/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64 \
        -L${INSTRUMENT_DIR} -lPforinst \
        sort.o -o sort

    CILK_NWORKERS=1 LD_LIBRARY_PATH=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64:/afs/ece/project/seth_group/ziqiliu/instrument-test \
        ./sort -r 2 -i /afs/ece/project/seth_group/pakha/pbbsbench/testData/sequenceData/data/randomSeq_100M_double

    mv pforinst.csv $TESTNAME.pforinst.csv

    echo "$TESTNAME done!"
    
    popd
done