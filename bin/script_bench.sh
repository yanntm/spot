#! /bin/bash

mkdir tests
rm tests/*.txt
rm res_tree.txt res_normal.txt

for i in 2 4 8 16 32 64 128 256 512 1024; do
  echo "Bench on states with $i variables"
  ./treetest --gen tests/$i.txt $i 1000000
  ./treetest --bench-tree tests/$i.txt >> tests/res_tree.txt
  ./treetest --bench-normal tests/$i.txt >> tests/res_normal.txt
done
