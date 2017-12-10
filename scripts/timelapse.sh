#!/bin/bash

DATA_DIR=/mnt/data/timelapse
mkdir -p "$DATA_DIR"
mkdir -p "$DATA_DIR/thumb"

TIMESTAMP=`date +"%s"`
IMAGE_FILE="$DATA_DIR/$TIMESTAMP.jpg"
THUMB_FILE="$DATA_DIR/$TIMESTAMP-thumb.jpg"

LD_LIBRARY_PATH=/opt/vc/lib /opt/vc/bin/raspistill -th 128:96:70 -n -ex auto -awb auto --raw -o "$IMAGE_FILE"

exiv2 -et "$IMAGE_FILE"
mv "$THUMB_FILE" "$DATA_DIR/thumb/"

# Compress current image.
/home/ubuntu/compress.sh "$DATA_DIR"
if [ $? -eq 0 ]; then
  IMAGE_FILE="$DATA_DIR/jpg/$TIMESTAMP.jpg"
fi

# Update Redis current frame key.
redis-cli set current.jpg "$IMAGE_FILE"
