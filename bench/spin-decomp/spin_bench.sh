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
TIMEOUT=600			# Duration of the timeout

# check for the usage
[ ! -d $PROPERTIES ] && echo "$PROPERTIES directory not found"
[ ! -d $PROPERTIES ] && exit 1

[ -d $OUTPUT ] && echo "$OUTPUT directory found, erase it before"
[ -d $OUTPUT ] && exit 1

# Walk accross the set of properties
for prop in $(ls properties); do
    echo "Processing $prop"

    # Results of time
    #  - 0   means no timeout 
    #  - !=0 means timeout
    property_timeout=0
    strong_timeout=0
    weak_timeout=0
    terminal_timeout=0

    # Go into the property directory
    cd $PROPERTIES/$prop

    # Launch all emptiness check and output it into the res directory
    mkdir -p $OUTPUT/$prop

    #  For the original property and cleanup
    spin -a -N  property.neverclaim *.pm
    cc -o pan pan.c
    timeout $TIMEOUT ./pan -a > $OUTPUT/$prop/property.neverclaim.txt
    property_timeout=$?
    echo $property_timeout > $OUTPUT/$prop/property.timeout
    rm -f pan* *.trail

    # There is a strong decomposition
    if [ -f strong.neverclaim ]; then
	spin -a -N  strong.neverclaim *.pm
	cc -o pan pan.c
	timeout $TIMEOUT ./pan -a > $OUTPUT/$prop/strong.neverclaim.txt
	strong_timeout=$?
	echo $strong_timeout > $OUTPUT/$prop/strong.timeout
	rm -f pan* *.trail
    fi;

    # There is a weak decomposition
    if [ -f weak.neverclaim ]; then
	spin -a -N  weak.neverclaim *.pm
	cc -o pan pan.c
	timeout $TIMEOUT ./pan -a > $OUTPUT/$prop/weak.neverclaim.txt
	weak_timeout=$?
	echo $weak_timeout > $OUTPUT/$prop/weak.timeout
	rm -f pan* *.trail
    fi;

    # There is a strong decomposition
    if [ -f terminal.neverclaim ]; then
	spin -a -N  terminal.neverclaim *.pm
	cc -o pan pan.c
	timeout $TIMEOUT ./pan -a > $OUTPUT/$prop/terminal.neverclaim.txt
	terminal_timeout=$?
	echo $terminal_timeout > $OUTPUT/$prop/terminal.timeout
	rm -f pan* *.trail
    fi;


    # Now we check the results in consistence with the original
    # results
    STATUS=$(cat status.txt)

    # Distinguish two cases : verified , unverified
    if [ "$STATUS" = "VERIFIED" ]; then

	# Strong must be verified
	if [ "$strong_timeout" = "0" ] &&  [ -f strong.neverclaim ]; then
	    res=$(grep 'errors:0' strong.neverclaim)
	    if [ "$res" != "" ]; then
		echo "ERROR: Strong should be verified $prop"
	    fi
	fi

	# Weak must be verified
	if [ "$weak_timeout" = "0" ] && [ -f weak.neverclaim ]; then
	    res=$(grep 'errors:0' weak.neverclaim)
	    if [ "$res" != "" ]; then
		echo "ERROR: Weak should be verified $prop"
	    fi
	fi

	# Terminal must be verified
	if [ "$terminal_timeout" = "0" ] && [ -f terminal.neverclaim ]; then
	    res=$(grep 'errors:0' terminal.neverclaim)
	    if [ "$res" != "" ]; then
		echo "ERROR: Terminal should be verified $prop"
	    fi
	fi

	# general  must be verified
	if [ "$property_timeout" = "0" ] && [ -f property.neverclaim ]; then
	    res=$(grep 'errors:0' property.neverclaim)
	    if [ "$res" != "" ]; then
		echo "ERROR: General should be verified $prop"
	    fi
	fi
    else
	# general  must be violated
	if [ "$property_timeout" = "0" ] && [ -f property.neverclaim ]; then
	    res=$(grep 'errors:1' property.neverclaim)
	    if [ "$res" != "" ]; then
		echo "ERROR: General should be violated $prop"
	    fi
	fi

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