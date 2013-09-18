#! /bin/bash
#
# This script only filter formula to match characteristics

whattocheck=$1
file=$2
result=$3

while read line; do
    # echo "$whattocheck    $file   $line"
    ./dve2check "$line"  ~/Desktop/at.4.dve -wait$whattocheck >> $result

    TMP=$(cat $result | awk -F',' '{print "+"$3}' | tr "\n" " ")
    RES=$(echo 0 "$TMP" | bc)
    echo $RES
    if [ "$RES" -gt "7200000" ]; then
    	exit 1;
    fi
done;
