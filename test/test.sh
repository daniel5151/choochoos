#!/bin/bash

set -e

folders=(k1 pingpong)

for folder in "${folders[@]}"; do
  echo "building ${folder}..."

  make clean >/dev/null
  make $@ USER_FOLDER="${folder}" release >/dev/null

  echo "running ${folder}..."
  ts7200 bin/choochoos.elf > "test/${folder}.actual"
done

for folder in "${folders[@]}"; do
  echo -n "checking $folder: "
  diff --strip-trailing-cr "test/$folder.actual" "test/$folder.expected"
  echo "outputs match"
done
