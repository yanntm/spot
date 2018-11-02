#! /bin/bash

#PBS -N tree_state_bench -M arthur.remaud@lrde.epita.fr -d /lrde/home/lrde-2019/aremaud/Documents/spot/bin -m ea 

mkdir /work/aremaud/tests
rm /work/aremaud/tests/*.txt
# rm /work/aremaud/tests/res_tree.txt /work/aremaud/tests/res_normal.txt

for i in 512; do
  echo "Bench on states with $i variables"
  ./treetest --gen /work/aremaud/tests/$i.txt $i 1000000
  ./treetest --bench-tree /work/aremaud/tests/$i.txt >> res_tree.txt
  ./treetest --bench-normal /work/aremaud/tests/$i.txt >> res_normal.txt
  rm /work/aremaud/tests/$i.txt
done
