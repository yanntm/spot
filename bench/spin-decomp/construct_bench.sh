#! /bin/sh
#  ------------------------------------------------------------------
#  File            : construct_bench.sh
#  Author          : Renault Etienne
#  Creation        : Tue Oct 16 13:23:18 2012
#  Last Revision   : Tue Oct 16 13:23:18 2012
#
#  Description     : this file produce the benchmark for spin
#  ------------------------------------------------------------------

tgba_analysis=../../src/tgbatest/tgba_analysis

#$( head -n3 allbenchs.txt > /tmp/1)
while read prop model status ltl; do
    # The current proposition treated
    echo $prop

    # Create dest folder
    mkdir -p properties/$prop

    # Copy status result
    echo $status > properties/$prop/status.txt

    # Copy ltl property
    echo $ltl > properties/$prop/property.ltl

    #
    # Here we focus on the promela models
    #
    tmp=$(echo $model | sed 's/.dve//g')
    [ ! -f ./models/$tmp.pm ] && continue
    cp ./models/$tmp.pm properties/$prop/$tmp.pm

    # The formula for promela model
    newltl=$ltl

    # Grab the atomic propositions used by this formula
    atomic_prop=$(cat properties/$prop/property.ltl | sed 's/[^"]*\("[^"]*"\)[^"]*/\1/g' | tr '"' ' ' )

    # All these atomic proposition are now translated into promela
    # style
    for i in $atomic_prop ; do
	left=$(echo $i | tr '[:upper:]' '[:lower:]' | sed 's/\./__/g')
	right=$( echo $i | tr '.' '@')
	echo '#define ' $left ' ( ' $right ' )'>> properties/$prop/pm_define.txt
	newltl=$(echo $newltl | sed "s/\"$i\"/$left/g")
    done;

    # Rewrite the model with the inclusion of #define ..
    cat properties/$prop/$tmp.pm >> properties/$prop/pm_define.txt
    mv properties/$prop/pm_define.txt  properties/$prop/$tmp.pm

    # Now we have the formula that is understandable by promela
    # we can produce never claim

    # Produce never claim for the original formula
    $tgba_analysis -dfa -m --spin "$newltl" > properties/$prop/property.neverclaim
    [ ! -s properties/$prop/property.neverclaim ]  && rm properties/$prop/property.neverclaim

    # Produce never claim for the terminal decomposition
    $tgba_analysis -dta -m --spin "$newltl" > properties/$prop/terminal.neverclaim
    [ ! -s properties/$prop/terminal.neverclaim ]  && rm properties/$prop/terminal.neverclaim
    [ ! -f properties/$prop/terminal.neverclaim ]  && echo "  ->Empty : terminal"

    # Produce never claim for weak decomposition
    $tgba_analysis -dwa -m --spin "$newltl" > properties/$prop/weak.neverclaim
    [ ! -s properties/$prop/weak.neverclaim ]  && rm properties/$prop/weak.neverclaim
    [ ! -f properties/$prop/weak.neverclaim ]  && echo "  ->Empty : weak"

    # Produce never claim for strong decomposition
    $tgba_analysis -dsa -m --spin "$newltl" > properties/$prop/strong.neverclaim
    [ ! -s properties/$prop/strong.neverclaim ]  && rm properties/$prop/strong.neverclaim
    [ ! -f properties/$prop/strong.neverclaim ]  && echo "  ->Empty : strong"
 done < allbenchs.txt
#/tmp/1
