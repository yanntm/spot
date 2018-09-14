#! /bin/bash

OUTPUTS_DIR="/lrde/cluster/adilisi/benchs"
MODEL_DIR="/work/adilisi/BEEM"
MODELCHECK_DIR="/work/adilisi/spot/tests/ltsmin"
NB_PROC=2
NP=4
HOSTFILE="$MODELCHECK_DIR"/hostfile.2

cd "$MODEL_DIR"

  for model in *.dve2C
  do
    echo "processing model $model"
    echo "$NB_PROC processes will be launched ..."

    mpirun --mca orte_abort_on_non_zero_status 0 \
    --mca btl_tcp_if_include bond0 --map-by node -np "$NP" \
    --hostfile "$HOSTFILE" \
    "$MODELCHECK_DIR"/modelcheck --model "$( basename $model )" \
    --has-deadlock --timer --csv --distribute -p "$NB_PROC" \
    --out "$OUTPUTS_DIR"/"$( basename $model .dve2C )".distributed."$NB_PROC" \
    --err "$OUTPUTS_DIR"/"$model".err."$NB_PROC"

    mpirun --mca orte_abort_on_non_zero_status 0 --mca btl_tcp_if_include bond0 --map-by node -np 1 \
    "$MODELCHECK_DIR"/modelcheck --model "$( basename $model )" \
    --has-deadlock --timer --csv -p "$NB_PROC" \
    > "$OUTPUTS_DIR"/"$( basename $model .dve2C )".parallel."$NB_PROC"
  done

NB_PROC=4

while [ "$NB_PROC" -le 12 ]
do

  for model in *.dve2C
  do
    echo "processing model $model"
    echo "$NB_PROC processes will be launched ..."

    mpirun --mca orte_abort_on_non_zero_status 0 \
    --mca btl_tcp_if_include bond0 --map-by node -np "$NP" \
    --hostfile "$HOSTFILE" \
    "$MODELCHECK_DIR"/modelcheck --model "$( basename $model )" \
    --has-deadlock --timer --csv --distribute -p "$NB_PROC" \
    --out "$OUTPUTS_DIR"/"$( basename $model .dve2C )".distributed."$NB_PROC" \
    --err "$OUTPUTS_DIR"/"$model".err."$NB_PROC"

    mpirun --mca orte_abort_on_non_zero_status 0 --mca btl_tcp_if_include bond0 --map-by node -np 1 \
    "$MODELCHECK_DIR"/modelcheck --model "$( basename $model )" \
    --has-deadlock --timer --csv -p "$NB_PROC" \
    > "$OUTPUTS_DIR"/"$( basename $model .dve2C )".parallel."$NB_PROC"
  done

  NB_PROC=$["$NB_PROC"+4]
done

cd "$OUTPUTS_DIR"
rm *.err.*

exit 0
