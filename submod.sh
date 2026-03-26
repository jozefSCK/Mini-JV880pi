#!/bin/bash
set -ex

# Update top-level modules as a baseline
git submodule update --init --recursive -f

# Use fixed master branch of circle-stdlib then re-update
cd circle-stdlib/
git checkout -f --recurse-submodules 1111eee # Matches Circle Step49
cd -

# Optional update submodules explicitly
cd circle-stdlib/libs/circle
git checkout -f --recurse-submodules f18c60fa38042ea7132533e658abfafd5bd63435
cd -
#cd circle-stdlib/libs/circle-newlib
#git checkout develop
#cd -
# Tvoja úprava pre KY-040 (cesta začína v src/)
echo "Kopirujem vylepseny ky040 kod zo src/custom_circle..."
cp src/custom_circle/ky040.cpp circle-stdlib/libs/circle/addon/input/ky040.cpp
cp src/custom_circle/ky040.h circle-stdlib/libs/circle/addon/input/ky040.h

echo "Hotovo, pokracujem v builde."
#spusti:-)
