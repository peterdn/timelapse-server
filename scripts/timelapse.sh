#!/bin/bash

DATA_DIR=/mnt/data/timelapse
mkdir -p "$DATA_DIR"

TIMESTAMP=`date +"%s"`
IMAGE_FILE="$DATA_DIR/$TIMESTAMP.jpg"
THUMB_FILE="$DATA_DIR/$TIMESTAMP-thumb.jpg"
CURRENT_FRAME="$DATA_DIR/current.jpg"
CURRENT_THUMB="$DATA_DIR/current-thumb.jpg"

LD_LIBRARY_PATH=/opt/vc/lib /opt/vc/bin/raspistill -th 128:96:70 -n -ex auto -awb auto --raw -o "$IMAGE_FILE"

exiv2 -et "$IMAGE_FILE"

ln -sf "$IMAGE_FILE" "$CURRENT_FRAME"
ln -sf "$THUMB_FILE" "$CURRENT_THUMB"

# Update Redis current frame key.
redis-cli set current.jpg "$IMAGE_FILE"
