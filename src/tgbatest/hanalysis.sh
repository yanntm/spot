#! /bin/sh
# This script is a wrapper for Hierarchy analysis 
# in term of WEAK, TERMINAL and GENERAL States 
LTL2TGBA=./ltl2tgba
LTL2OPTION="-R3 -Rm -r3 -r1 -ha"

# Check usage 
if [ "$#" -gt "2" ]; then 
    echo "usage : hanalysis [formula | -f filename]"
    exit 1
fi;

if [ "$#" -eq "0" ]; then 
    echo "usage : hanalysis [formula | -f filename]"
    exit 1
fi;

#  Read from a file 
if [ "$1" == "-f" ];then 
    while read line ; do 
	if [ "$(echo $line | grep "#")" != "" ]; then 
	    continue;
	fi
	tmp=$($LTL2TGBA $LTL2OPTION "$line" | head -n1)
	echo "$tmp\t$line" 
    done < $2
else
    # Just check a formula 
    tmp=$($LTL2TGBA $LTL2OPTION "$@" | head -n1)
    echo "$tmp\t$@" 
fi;
