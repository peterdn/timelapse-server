#!/bin/bash

if [ "$1" == "" ]; then
  echo "Usage: compress.sh <directory>"
  exit 255
fi

source_dir=$1
raw_dir="$1/raw"
jpg_dir="$1/jpg"

echo "Compressing $source_dir..."

mkdir -p "$raw_dir"
mkdir -p "$jpg_dir"

for f in $source_dir/*.jpg; do
  if [ $(echo $f | grep -E "thumb|current") ]; then
    echo "Skipping $f..."
    continue
  fi

  echo "Compressing $f..."

  filename=$(basename -s .jpg "$f")
  raw_filename="$raw_dir/$filename.raw"
  compressed_filename="$raw_dir/$filename-compressed.raw"
  jpg_filename="$jpg_dir/$filename.jpg"

  if [ -e "$raw_filename" ] || [ -e "$compressed_filename" ] || [ -e "$jpg_filename" ]; then
    echo "An output file of $f already exists!"
    exit 255
  fi

  head -c -6404096 "$f" > "$jpg_filename"
  tail -c 6404096 "$f" > "$raw_filename"

  # Verify these file have been split correctly.
  original_md5=$(md5sum "$f" | awk '{ print $1 }')
  combined_md5=$(cat $jpg_filename $raw_filename | md5sum | awk '{ print $1 }')

  if [ $original_md5 != $combined_md5 ]; then
    echo "ERROR: file $f not split correctly!"
    exit 255
  fi

  # Compress file.
  mono /home/ubuntu/CompressRaw.exe "$raw_filename" "$compressed_filename"
  if [ $? -ne 0 ]; then
    echo "ERROR: failed to compress file $f"
    exit 255
  fi

  # Delete uncompressed raw files.
  echo "Deleting $f and $raw_filename..."
  rm -f "$f"
  rm -f "$raw_filename"
done

