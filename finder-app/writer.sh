#!/bin/sh
# Tester script for assignment 1
# Author: Julio Cesar

set -e
set -u

# Verify if the arg numbers is correct
if [ $# -ne 2 ]
then
	        echo "Usage: $0 <file_path> <write_string>"
		        exit 1
fi

WRITEFILE=$1
WRITESTR=$2

mkdir -p "$(dirname "$WRITEFILE")" || {
	echo "Error: could not create directory."
	exit 1
}
printf "%s\n" "$WRITESTR" > "$WRITEFILE" || {
	echo "Error: could not write to file."
        exit 1
}
