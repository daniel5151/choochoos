#!/bin/bash

set -e

echo "building k1..."

make clean >/dev/null
make $@ release >/dev/null

echo "running k1..."
ts7200 bin/choochoos.elf > "test/k1.actual"

echo "building k2..."

make clean >/dev/null
make $@ USER_FOLDER=k2 release >/dev/null

echo "running k2..."

ts7200 bin/choochoos.elf > "test/k2.actual"

echo -n "checking k1: "
diff --strip-trailing-cr "test/k1.actual" "test/k1.expected"
echo "outputs match"
echo -n "checking k2: "
diff --strip-trailing-cr "test/k2.actual" "test/k2.expected"
echo "outputs match"
