#!/bin/sh
# Tester script for assignment 1
# Author: Julio Cesar

set -e
set -u

FILEDIR=/tmp/aesd
SEARCHSTR=SAMPLE
NFILE=0
NLINE=0

# Verify if the arg numbers is correct
if [ $# -ne 2 ]
then
	echo "Usage: $0 <directory> <search_string>"
	exit 1
fi

# Verify if the directory path is valid
if [ ! -d "$1" ]
then
	echo "The directory "$1" not exist"
	exit 1
fi

FILEDIR=$1
SEARCHSTR=$2
NFILE=$(find "$FILEDIR" -type f | wc -l)

# Get the Number of Lines with the String Searchedi
NLINE=$(grep -r -c "$SEARCHSTR" "$FILEDIR" | awk -F: '{sum += $2} END {print sum}')

echo "The number of files are "$NFILE" and the number of matching lines are $NLINE"
