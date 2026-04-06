#!/bin/sh

if [ $# -ne 2 ]; then
  echo "Error: Two parameters are required."
  echo "Usage: $0 <writefile> <writestr>"
  exit 1
fi

writefile="$1"
writestr="$2"

dirpath=$(dirname "$writefile")
if [ ! -d "$dirpath" ]; then
  mkdir -p "$dirpath" 2>/dev/null
  if [ $? -ne 0 ]; then
    echo "Error: Could not create directory path $dirpath"
    exit 1
  fi
fi

echo "$writestr" > "$writefile" 2>/dev/null
if [ $? -ne 0 ]; then
  echo "Error: Could not create or write to file $writefile"
  exit 1
fi

echo "Successfully wrote $writestr to $writefile"
exit 0