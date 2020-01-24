#!/bin/bash

set -e

make clean
make $@ release

echo "building k1..."

echo "running k1..."
ts7200 bin/choochoos.elf > "test/actual"
diff --strip-trailing-cr "test/actual" "test/expected"

echo "outputs match"
