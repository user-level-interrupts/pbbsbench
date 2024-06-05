#!/bin/bash
# set -e
INSTRUMENT_DIR=/afs/ece/project/seth_group/ziqiliu/instrument-test
CHEETAH_DIR=/afs/ece/project/seth_group/ziqiliu/cheetah/build/

NAMES=("Chan05" "CKNN" "naive" "octTree")

for TESTNAME in "${NAMES[@]}"
do
    pushd $TESTNAME

    clang++ -gdwarf-2 -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -fforkd=lazy -ftapir=serial \
        --opencilk-resource-dir=../../../../cheetah/build/ \
        --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0 \
        -mcx16 -O3 -std=c++17 -DNDEBUG -I . \
        -Xclang -fpass-plugin=$INSTRUMENT_DIR/instrument-pass.so \
        -include neighbors.h -o neighbors.o -c ../bench/neighborsTime.C

    clang++ -gdwarf-2 -mcx16 -O3 -std=c++17 \
        -fforkd=lazy -ftapir=serial -ldl -fuse-ld=lld \
        -L/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64 \
        -L${INSTRUMENT_DIR} -lInstrument \
        neighbors.o -o neighbors

    CILK_NWORKERS=1 LD_LIBRARY_PATH=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64:/afs/ece/project/seth_group/ziqiliu/instrument-test \
        ./neighbors -r 2 -i /afs/ece/project/seth_group/pakha/pbbsbench/testData/geometryData/data/2Dkuzmin_1000000

    mv ef.csv $TESTNAME.ef.csv
    mv dac.csv $TESTNAME.dac.csv

    ###### run pforinst instrumentation ##############
    clang++ -gdwarf-2 -DBUILTIN -DPARLAY_OPENCILK -mllvm -experimental-debug-variable-locations=false \
        -fforkd=lazy -ftapir=serial \
        --opencilk-resource-dir=../../../../cheetah/build/ \
        --gcc-toolchain=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0 \
        -mcx16 -O3 -std=c++17 -DNDEBUG -I . \
        -Xclang -fpass-plugin=$INSTRUMENT_DIR/libBuiltinIntrinsic.so \
        -include neighbors.h -o neighbors.o -c ../bench/neighborsTime.C

    clang++ -gdwarf-2 -mcx16 -O3 -std=c++17 \
        -fforkd=lazy -ftapir=serial -ldl -fuse-ld=lld \
        -L/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64 \
        -L${INSTRUMENT_DIR} -lPforinst \
        neighbors.o -o neighbors

    CILK_NWORKERS=1 LD_LIBRARY_PATH=/afs/ece/project/seth_group/ziqiliu/GCC-12.2.0/lib64:/afs/ece/project/seth_group/ziqiliu/instrument-test \
        ./neighbors -r 2 -i /afs/ece/project/seth_group/pakha/pbbsbench/testData/geometryData/data/2Dkuzmin_1000000

    mv pforinst.csv $TESTNAME.pforinst.csv

    echo "$TESTNAME done!"
    popd
done