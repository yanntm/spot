# This file perform a factorisation between three output extracted
# of the decomposition

#
# Compare two output of the bench
#
compare_two ()
{
    arg1=$1
    arg2=$2

    hasdecomp1=$(echo $arg1 | grep ',NODECOMP,')
    hasdecomp2=$(echo $arg2 | grep ',NODECOMP,')

    # There is no valid decomposition
    if [ "$hasdecomp1" != "" ] && [ "$hasdecomp2" != "" ] ; then
	echo $arg1
	return;
    fi;
    # Decomposition 1 is not valid
    if [ "$hasdecomp1" != "" ] && [ "$hasdecomp2" = "" ] ; then
	echo $arg2
	return ;
    fi;
    # Decomposition 2 is not valid
    if [ "$hasdecomp1" = "" ] && [ "$hasdecomp2" != "" ] ; then
	echo $arg1
	return;
    fi;

    # if one of the two is violated and not the other
    # choose the violated one
    isviolatedarg1=$(echo $arg1 | grep ',VIOLATED,')
    isviolatedarg2=$(echo $arg2 | grep ',VIOLATED,')

    # if only one is violated it must be returned

    # Arg1 is unusefull
    if [ "$isviolatedarg1" = "" ] && [ "$isviolatedarg2" != "" ] ; then
	echo $arg2
	return;
    fi;
    # Arg2 is unusefull
    if [ "$isviolatedarg1" != "" ] && [ "$isviolatedarg2" = "" ] ; then
	echo $arg1
	return;
    fi;

    # Both decomp are valid (Violated/Verified) choose the best one in
    # term of time
    timearg1=$(echo $arg1 | awk -F"," '{print $4}')
    timearg2=$(echo $arg2 | awk -F"," '{print $4}')


    # If violated it's the shortest
    if [ "$isviolatedarg1" != "" ] ; then
	if [ "$timearg1" -lt "$timearg2" ] ; then
	    echo $arg1;
	    return ;
	else
	    echo $arg2
	    return ;
	fi
    else
	if [ "$timearg1" -gt "$timearg2" ] ; then
	    echo $arg1;
	    return;
	else
	    echo $arg2
	    return;
	fi
    fi
}

#
# Compare three output of the bench
#
compare_three ()
{
    arg1=$1
    arg2=$2
    arg3=$3

    restmp=$(compare_two "$arg1" "$arg2")
    compare_two "$arg3" "$restmp"
}

if [ "$#" -ne 3 ]; then
    echo "usage: mergedecomp.sh file1 file2 file3"
    exit 1
fi

# get the number of line in each decomposition
lineno=$(wc -l $1 | awk '{print $1}')
lineno=$((lineno+1))
i=1

while [ "$i" -ne "$lineno" ]
do
    var1=$(head -n"$i"  "$1" | tail -n1)
    var2=$(head -n"$i"  "$2" | tail -n1)
    var3=$(head -n"$i"  "$3" | tail -n1)

    # Check if it's a result line or if it's a
    # comment line
    tmp=$(echo $var1 | grep "#")
    if [ "$tmp" != "" ] ; then
	echo $var1
	i=$(($i+1))
	continue;
    fi;
    compare_three "$var3" "$var2" "$var1"
    i=$(($i+1))
done;
