#!/bin/bash

set -e

echo "running unit tests..."
make unit_tests

folders=(k1 k2 test_msgpass test_clock k3)

for folder in "${folders[@]}"; do
  echo "building ${folder}..."

  make -j $@ TARGET="${folder}" TESTS=1 >/dev/null

  exe="${folder}.elf"

  echo "running ${folder}..."

  input="test/${folder}.input"
  if [[ -f $input ]]; then
       tr '\n' '\r' < "$input" | ts7200 "$exe" > "test/${folder}.actual";
    else
      ts7200 "$exe" > "test/${folder}.actual";
    fi
done

for folder in "${folders[@]}"; do
  echo -n "checking $folder: "
  actual="test/$folder.actual"
  expected="test/$folder.expected"
  if [[ -f $expected ]]; then
      diff --strip-trailing-cr "$actual" "$expected"
      echo "outputs match"
  else
      echo "$expected does not exist, populating"
      cp -v "$actual" "$expected"
  fi
done

echo -n "validating k3: "
python3 test/k3_expected.py > test/k3_expected.py.out
diff --strip-trailing-cr "test/k3.actual" "test/k3_expected.py.out"
echo "ok"
