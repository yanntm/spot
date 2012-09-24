#! /bin/sh

. ./defs

RANDLTL="../../src/bin/randltl"
SCC_ANA="../../src/tgbatest/scc_analysis"
KIND="../../src/ltltest/kind"
resfile=$(mktemp /tmp/randscc_analysis.XXXXXX)


$RANDLTL -r -8 -n 10000  --seed 10 a b c |  $SCC_ANA - > $resfile

#cat $resfile
N=$(cat $resfile | grep '#' | wc -l)
echo Processing over $N formulas

# Processing terminal SCC
tmp=$(cat $resfile | grep -v '#' | grep -v '\-\-' | awk -F',' '{print $5}' | tr '\n' '+')
tmp="$tmp 0"
tmp=$(echo $tmp | bc)
echo "  Number of terminal SCC:" $tmp

# Processing non accepting SCC 
tmp=$(cat $resfile | grep -v '#' | grep -v '\-\-' | awk -F',' '{print $6}' | tr '\n' '+')
tmp="$tmp 0"
tmp=$(echo $tmp | bc)
echo "  Number of Strong SCC:" $tmp

# Processing non accepting SCC 
tmp=$(cat $resfile | grep -v '#' | grep -v '\-\-' | tr "#" " "| awk -F',' '{print $1}' | tr '\n' '+')
tmp="$tmp 0"
tmp=$(echo $tmp | bc)
echo "  Number of non accepting SCC:" $tmp

# Processing cycle weak  SCC
tmp=$(cat $resfile | grep -v '#' | grep -v '\-\-' | awk -F',' '{print $4}' | tr '\n' '+')
tmp="$tmp 0"
tmp=$(echo $tmp | bc)
echo "  Number of Weak SCC (cycles):" $tmp

# Processing structural weak  SCC
tmp=$(cat $resfile | grep -v '#' | grep -v '\-\-' | awk -F',' '{print $3}' | tr '\n' '+')
tmp="$tmp 0"
tmp=$(echo $tmp | bc)
echo "  Number of Weak SCC (structural heuristic):" $tmp


# Processing structural weak  SCC
tmp=$(cat $resfile | grep -v '#' | grep -v '\-\-' | awk -F',' '{print $2}' | tr '\n' '+')
tmp="$tmp 0"
tmp=$(echo $tmp | bc)
echo "  Number of Weak SCC (syntactic heuristic):" $tmp


# Process syntax at original stage
SAFETY=0
GUARANTEE=0
OBLIGATION=0
PERSISTENCE=0
RECURRENCE=0

while read line ; do 
    # Check only good lines 
    tmp=$(echo $line | grep '#')
    [ "$tmp" == "" ] && continue;
    tmp=$(echo $line | awk -F',' '{print $7}' | tr '\n' ' ')
    
    # Process kind over it
    type=$($KIND "$tmp")

    tmp=$(echo $type | grep guarantee)
    if [ "$tmp" != "" ]; then 
	GUARANTEE=$((GUARANTEE + 1))
	continue
    fi

    tmp=$(echo $type | grep safety)
    if [ "$tmp" != "" ]; then 
	SAFETY=$((SAFETY + 1))
	continue
    fi

    tmp=$(echo $type | grep obligation)
    if [ "$tmp" != "" ]; then 
	OBLIGATION=$((OBLIGATION + 1))
	continue
    fi

    tmp=$(echo $type | grep persistence)
    if [ "$tmp" != "" ]; then 
	PERSISTENCE=$((PERSISTENCE + 1))
	continue
    fi

    tmp=$(echo $type | grep recurrence)
    if [ "$tmp" != "" ]; then 
	RECURRENCE=$((RECURRENCE + 1))
	continue
    fi
done < $resfile

echo 
echo "Syntaxic type of formula given:"
echo "  Guarantee   : $GUARANTEE"
echo "  Safety      : $SAFETY" 
echo "  Obligation  : $OBLIGATION"
echo "  Persistence : $PERSISTENCE"
echo "  Recurrence  : $RECURRENCE"
echo "  General     :" $( echo "$N - $GUARANTEE - $SAFETY - $OBLIGATION - $PERSISTENCE - $RECURRENCE" | bc)



#mv $resfile scc_analysis_results.txt