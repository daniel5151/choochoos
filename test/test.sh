#!/bin/bash

set -e



folders=(k1 pingpong k2)

for folder in "${folders[@]}"; do
  echo "building ${folder}..."

  make clean >/dev/null
  make $@ USER_FOLDER="${folder}" release >/dev/null

  echo "running ${folder}..."

  input="test/${folder}.input"
  if [[ -f $input ]]; then
      ts7200 bin/choochoos.elf < "$input" > "test/${folder}.actual";
    else
      ts7200 bin/choochoos.elf > "test/${folder}.actual";
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
