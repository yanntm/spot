#! /bin/sh
#  ------------------------------------------------------------------
#  File            : spin_bench.sh
#  Author          : Renault Etienne
#  Creation        : Tue Oct 16 13:48:46 2012
#  Last Revision   : Tue Oct 16 13:48:46 2012
#
#  Description     : This file is used to launch the bench using
#                    spin
#  ------------------------------------------------------------------

PROPERTIES=$(pwd)/properties
OUTPUT=$(pwd)/properties-result

# check for the usage
[ ! -d $PROPERTIES ] && echo "$PROPERTIES directory not found"
[ ! -d $PROPERTIES ] && exit 1

[ -d $OUTPUT ] && echo "$OUTPUT directory found, erase it before"
[ -d $OUTPUT ] && exit 1

# Walk accross the set of properties
for prop in $(ls properties); do
    echo "Processing $prop"

    # Go into the property directory
    cd $PROPERTIES/$prop

    # Launch all emptiness check and output it into the res directory
    mkdir -p $OUTPUT/$prop

    #  For the original property and cleanup
    spin -a -N  property.neverclaim *.pm
    cc -o pan pan.c
    ./pan -a > $OUTPUT/$prop/property.neverclaim.txt
    rm -f pan* *.trail

    # There is a strong decomposition
    if [ -f strong.neverclaim ]; then
	spin -a -N  strong.neverclaim *.pm
	cc -o pan pan.c
	./pan -a > $OUTPUT/$prop/strong.neverclaim.txt
	rm -f pan* *.trail
    fi;

    # There is a weak decomposition
    if [ -f weak.neverclaim ]; then
	spin -a -N  weak.neverclaim *.pm
	cc -o pan pan.c
	./pan -a > $OUTPUT/$prop/weak.neverclaim.txt
	rm -f pan* *.trail
    fi;

    # There is a strong decomposition
    if [ -f terminal.neverclaim ]; then
	spin -a -N  terminal.neverclaim *.pm
	cc -o pan pan.c
	./pan -a > $OUTPUT/$prop/terminal.neverclaim.txt
	rm -f pan* *.trail
    fi;


    # Now we check the results in consistence with the original
    # results
    STATUS=$(cat status.txt)

    # Distinguish two cases : verified , unverified
    if [ "$STATUS" = "VERIFIED" ]; then

	# Strong must be verified
	if [ -f strong.neverclaim ]; then
	    res=$(grep 'errors:0' strong.neverclaim)
	    if [ "$res" != "" ]; then
		echo "ERROR: Strong should be verified $prop"
	    fi
	fi

	# Weak must be verified
	if [ -f weak.neverclaim ]; then
	    res=$(grep 'errors:0' weak.neverclaim)
	    if [ "$res" != "" ]; then
		echo "ERROR: Weak should be verified $prop"
	    fi
	fi

	# Terminal must be verified
	if [ -f terminal.neverclaim ]; then
	    res=$(grep 'errors:0' terminal.neverclaim)
	    if [ "$res" != "" ]; then
		echo "ERROR: Weak should be verified $prop"
	    fi
	fi
    else
	istermviolated=false
	isweakviolated=false
	isstrongviolated=false

	# Strong must be verified
	if [ -f strong.neverclaim ]; then
	    res=$(grep 'errors:1' strong.neverclaim)
	    if [ "$res" != "" ]; then
		istermviolated=true
	    fi
	fi

	# Weak must be verified
	if [ -f weak.neverclaim ]; then
	    res=$(grep 'errors:1' weak.neverclaim)
	    if [ "$res" != "" ]; then
		isweakviolated=true
	    fi
	fi

	# Terminal must be verified
	if [ -f terminal.neverclaim ]; then
	    res=$(grep 'errors:1' terminal.neverclaim)
	    if [ "$res" != "" ]; then
		isstrongviolated=true
	    fi
	fi
    fi

    # Backtrack from the property
    cd - 1>/dev/null
done