#!/bin/bash
# Builds the refactoring check: the port at <commit> (namespace rr_old) against the working tree.
# usage: port/tools/equivalence/build.sh <commit>; then run port/build/eq/eq from the repository root
set -e
COMMIT=${1:?commit}
ROOT=$(git rev-parse --show-toplevel)
HERE=$ROOT/port/tools/equivalence
OUT=$ROOT/port/build/eq
rm -rf $OUT && mkdir -p $OUT/old $OUT/o $OUT/n
git -C $ROOT archive $COMMIT port/src | tar -x -C $OUT/old
for f in $OUT/old/port/src/*.cpp; do g++ -std=c++17 -O2 -Drr=rr_old -I $OUT/old/port/src -c $f -o $OUT/o/$(basename $f .cpp).o; done
g++ -std=c++17 -O2 -Drr=rr_old -DSIDE=old -I $OUT/old/port/src -c $HERE/side.cpp -o $OUT/o/side.o
for f in $ROOT/port/src/*.cpp; do g++ -std=c++17 -O2 -I $ROOT/port/src -c $f -o $OUT/n/$(basename $f .cpp).o; done
g++ -std=c++17 -O2 -DSIDE=new -I $ROOT/port/src -c $HERE/side.cpp -o $OUT/n/side.o
g++ -O2 -I $HERE $HERE/eq.cpp $OUT/o/*.o $OUT/n/*.o -o $OUT/eq
