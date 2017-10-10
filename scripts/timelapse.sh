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

# Get size of current file in bytes.
IMAGE_SIZE=$(wc -c < "$IMAGE_FILE")

# Get available space on disk in MB.
DISK_FREE=$(df "$DATA_DIR" -m --output=avail | tail -n1)

# Dweet telemetry metrics.
DWEET_THING="gg14-timelapse-pi"
curl -m 5 "https://dweet.io/dweet/for/$DWEET_THING?timestamp=$TIMESTAMP&imageSize=$IMAGE_SIZE&diskFreeMB=$DISK_FREE"
