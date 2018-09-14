#! /bin/bash

OUTPUTS_DIR="/lrde/cluster/adilisi/benchs"
MODEL_DIR="/work/adilisi/BEEM"
MODELCHECK_DIR="/work/adilisi/spot/tests/ltsmin"
NB_PROC=1

cd "$MODEL_DIR"

for model in *.dve2C
do
    echo "processing model $model"

    mpirun --mca orte_abort_on_non_zero_status 0 -np 1 \
    "$MODELCHECK_DIR"/modelcheck --model "$( basename $model )" \
    --has-deadlock --timer --csv \
    > "$OUTPUTS_DIR"/"$( basename $model .dve2C )".sequential."$NB_PROC"
done

cd "$OUTPUTS_DIR"
rm *.err.*

exit 0
