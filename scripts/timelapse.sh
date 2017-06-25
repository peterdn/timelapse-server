#!/bin/bash

TIMESTAMP=`date +"%s"`
IMAGE_FILE=/mnt/data/timelapse/$TIMESTAMP.jpg
CURRENT_TIMESTAMP_FILE=/mnt/data/timelapse/current.txt

LD_LIBRARY_PATH=/opt/vc/lib /opt/vc/bin/raspistill -th 128:96:70 -n -rot 180 -ex auto -awb auto -o $IMAGE_FILE

exiv2 -et $IMAGE_FILE

echo $TIMESTAMP > $CURRENT_TIMESTAMP_FILE
