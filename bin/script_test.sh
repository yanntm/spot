#! /bin/bash

#PBS -N tree_state_tests -M arthur.remaud@lrde.epita.fr -d /lrde/home/lrde-2019/aremaud/Documents/spot/bin -m ea 

mkdir /work/aremaud/tests
rm /work/aremaud/tests/*.txt

for i in 2 4 8 16 32 64 128 256; do
  echo "Tests on states with $i variables"
  ./treetest --gen /work/aremaud/tests/$i.txt $i 1000000
  ./treetest --check-tree /work/aremaud/tests/$i.txt > /work/aremaud/tests/tmp_tree.csv
  ./treetest --check-normal /work/aremaud/tests/$i.txt > /work/aremaud/tests/tmp_normal.csv

  sort /work/aremaud/tests/tmp_tree.csv -o /work/aremaud/tests/tmp_tree.csv
  # cp /work/aremaud/tests/tmp_tree.csv res_hash.csv
  sort /work/aremaud/tests/tmp_normal.csv -o /work/aremaud/tests/tmp_normal.csv
  diff /work/aremaud/tests/tmp_tree.csv /work/aremaud/tests/tmp_normal.csv &> /lrde/home/lrde-2019/aremaud/Documents/spot/bin/tests/diff$i.txt

  rm /work/aremaud/tests/tmp_tree.csv /work/aremaud/tests/tmp_normal.csv
done
