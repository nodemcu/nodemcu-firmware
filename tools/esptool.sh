#!/bin/sh
BASEDIR=$(dirname $(readlink -f "$0"))
ret=`python -c 'import sys; print("%i" % (sys.hexversion<0x03000000))'`
if [ $ret -eq 0 ]; then
    #Retry python 2
	ret=`python2 -c 'import sys; print("%i" % (sys.hexversion<0x03000000))'`
	if [ $ret -eq 0 ]; then
	    echo "Cannot find python2."
	else 
	    #Ok
		python2 $BASEDIR/esptool.py $@
	fi
else 
    #Ok
	python $BASEDIR/esptool.py $@
fi