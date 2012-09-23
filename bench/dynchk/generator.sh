#! /bin/sh
# This script parse a model to extract the atomic propositions
# and then generate the associated formula (syntactic)

# FIXME 
RANDLTL=../../src/ltltest/randltl


if [ "$#" -lt 4 ]; then 
    echo "Usage : benchs.sh size type [number] ap"
    echo "  - size :  the length (syntactic) of formula"
    echo "  - type :  1  (Strong-Terminal)        F a R .. R F n"
    echo "            2  (Strong-Terminal)        F a W .. W F n"
    echo "            3  (Weak-Terminal)          F a -> .. -> F n"
    echo "            4  (Strong-Weak-Terminal)   F a R .. R (FG n | m)"
    echo "            5  (Strong-Weak-Terminal)   F a W .. W (FG n | m)"
    echo "            6  Random Strong Commut (PSL)"
    echo "            7  Random Weak Commut   (PSL)"
    echo "            8  Random Commut   (PSL)"
#     echo "            8  Random Strong Commut (LTL)"
#     echo "            9  Random Weak Commut   (LTL)"
    echo " - number : the number of formula to generate (for type >5)" 
    exit 1;
fi 

# Fix all variables 
SIZE=$1
shift
TYPE=$1
shift
NUMBER=$1
shift 
AP=$@



# # Check consistency
# if [ "$TYPE" -le 3 ]; then 
#     if [ "$SIZE" -lt 1 ]; then 
# 	echo "Usage : size must be positive for given type ($TYPE)"
# 	exit 1;
# 	fi;
# else
#     if [ "$TYPE" -le 5 ]; then 
# 	if [ "$SIZE" -lt 3 ]; then 
# 	    echo "Usage : size must be greater than 3 for given type ($TYPE)"
# 	    exit 1;
# 	fi;
#     fi;
# fi;

# get atomic propositions of the net and also the number
#AP=$(cat $MODEL | grep "#place" | awk  '{ print $2}' | tr "\n" " ")
#AP="a z e r t y"
AP_SIZE=$(echo $AP | tr " " "\n" | wc -l)

# Declare an array containing all these AP
declare -a tab=($AP)

# The Final result
RESULT=

case $TYPE in
    ## F a R .. R F n
    1)  while [ "$SIZE" -ne "0" ] ; do
	  SIZE=$((SIZE -1))
	  index=$((RANDOM % AP_SIZE))
	  RESULT="$RESULT F \"${tab[index]}\""
	  if [ "$SIZE" -ne "0" ] ; then
	      RESULT="$RESULT R"
	  fi;
	done;
	;;
    ## F a W .. W F n
    2)  while [ "$SIZE" -ne "0" ] ; do
	  SIZE=$((SIZE -1))
	  index=$((RANDOM % AP_SIZE))
	  RESULT="$RESULT F \"${tab[index]}\""
	  if [ "$SIZE" -ne "0" ] ; then
	      RESULT="$RESULT W"
	  fi;
	done;
	;;
    ## F a -> .. -> F n
    3)  while [ "$SIZE" -ne "0" ] ; do
	  SIZE=$((SIZE -1))
	  index=$((RANDOM % AP_SIZE))
	  RESULT="$RESULT F \"${tab[index]}\""
	  if [ "$SIZE" -ne "0" ] ; then
	      RESULT="$RESULT ->"
	  fi;
	done;
	;;
    ## F a R .. R (FG n | m)
    4)  SIZE=$((SIZE - 2))
        while [ "$SIZE" -ne "0" ] ; do
	  SIZE=$((SIZE -1))
	  index=$((RANDOM % AP_SIZE))
	  RESULT="$RESULT F \"${tab[index]}\""
	  RESULT="$RESULT R"
	done;
	index=$((RANDOM % AP_SIZE))
	RESULT="$RESULT (FG \"${tab[index]}\" |"
	index=$((RANDOM % AP_SIZE))
	RESULT="$RESULT \"${tab[index]}\")"
	;;
    ## F a W .. W (FG n | m)
    5)  SIZE=$((SIZE - 2))
        while [ "$SIZE" -ne "0" ] ; do
	  SIZE=$((SIZE -1))
	  index=$((RANDOM % AP_SIZE))
	  RESULT="$RESULT F \"${tab[index]}\""
	  RESULT="$RESULT W"
	done;
	index=$((RANDOM % AP_SIZE))
	RESULT="$RESULT (FG \"${tab[index]}\" |"
	index=$((RANDOM % AP_SIZE))
	RESULT="$RESULT \"${tab[index]}\")"
	;;
  ## Random Strong commut  
    6) 	$RANDLTL -8 -u  -s $((RANDOM)) -P   -C -g  -r 10 -f $SIZE  -F $NUMBER  $AP
	exit 0;
	;;
    7) $RANDLTL -8 -u  -s $((RANDOM)) -P   -C -w -r 10 -f $SIZE   -F $NUMBER  $AP
	exit 0;
	;;
    8) $RANDLTL -8 -u  -s $((RANDOM%1000)) -P   -C -r 10 -f $SIZE   -F $NUMBER  $AP
	exit 0;
	;;
#   8) $RANDLTL   -C -g  -r 10 -f 15  -F $SIZE  $AP
# 	;;
#   9) $RANDLTL   -C -w  -r 10 -f 15  -F $SIZE  $AP
#	;;
esac

echo $RESULT