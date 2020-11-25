#!/bin/bash

#inputs are: POI ZOOM OUTPUTDIR
# e.g.: ./runscript.sh FN42IK44LP 16 out1
python tile.py $1 $2 
./tidyscript.sh $3