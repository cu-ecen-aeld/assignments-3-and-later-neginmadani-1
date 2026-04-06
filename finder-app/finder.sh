#!/bin/sh

if [ $# -ne 2 ]; then
  echo "Error: Two parameters are required."
  echo "Usage: $0 <filesdir> <searchstr>"
  exit 1
fi

filesdir="$1"
searchstr="$2"

if [ ! -d "$filesdir" ]; then
  echo "$filesdir is not a directory."
  exit 1
fi

lines_count=$(grep -r -h "$searchstr" "$filesdir" | wc -l) 
files_count=$(grep -r -l "$searchstr" "$filesdir" | wc -l) 

echo "The number of files are $files_count and the number of matching lines are $lines_count"

exit 0