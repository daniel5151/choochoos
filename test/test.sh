#!/bin/bash

set -e

echo "running unit tests..."
make unit_tests

folders=(k1 pingpong k2)

for folder in "${folders[@]}"; do
  echo "building ${folder}..."

  make $@ CURRENT_ASSIGNMENT="${folder}" >/dev/null

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
