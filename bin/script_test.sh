#! /bin/bash

mkdir tests
rm tests/*.txt

for i in 2 4 8 16 32 64 128 256; do
  echo "Tests on states with $i variables"
  ./treetest --gen tests/$i.txt $i 1000000
  ./treetest --check-tree tests/$i.txt > tests/tmp_tree.csv
  ./treetest --check-normal tests/$i.txt > tests/tmp_normal.csv

  sort tests/tmp_tree.csv -o tests/tmp_tree.csv
  sort tests/tmp_normal.csv -o tests/tmp_normal.csv
  diff tests/tmp_tree.csv tests/tmp_normal.csv &> tests/diff$i.txt

  rm tests/tmp_tree.csv tests/tmp_normal.csv
done
