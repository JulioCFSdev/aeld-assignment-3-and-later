#!/bin/sh
#!/bin/bash
# Tester script for assignment 1 and assignment 2
# Author: Siddhant Jajoo

set -e
set -u

NUMFILES=10
WRITESTR=AELD_IS_FUN
WRITEDIR=/tmp/aeld-data
CONF_DIR=/etc/finder-app/conf
RESULT_FILE=/tmp/assignment4-result.txt

username=$(cat "$CONF_DIR/username.txt")
assignment=$(cat "$CONF_DIR/assignment.txt")

if [ "$#" -ge 3 ]; then
    NUMFILES=$1
    WRITESTR=$2
    WRITEDIR="/tmp/aeld-data/$3"
elif [ "$#" -ge 1 ]; then
    NUMFILES=$1
fi

case "$NUMFILES" in
    ''|*[!0-9]*|0)
        echo "Error: NUMFILES must be a positive integer"
        exit 1
        ;;
esac

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are ${NUMFILES}"

echo "Writing ${NUMFILES} files containing string ${WRITESTR} to ${WRITEDIR}"

rm -rf "$WRITEDIR"

if [ "$assignment" != "assignment1" ]; then
    mkdir -p "$WRITEDIR"
fi

i=1
while [ "$i" -le "$NUMFILES" ]; do
    writer "$WRITEDIR/${username}${i}.txt" "$WRITESTR"
    i=$((i + 1))
done

OUTPUTSTRING=$(finder.sh "$WRITEDIR" "$WRITESTR")

printf '%s\n' "$OUTPUTSTRING" | tee "$RESULT_FILE"

rm -rf /tmp/aeld-data

if printf '%s\n' "$OUTPUTSTRING" | grep -Fq "$MATCHSTR"; then
    echo "success"
    exit 0
else
    echo "failed: expected $MATCHSTR but found: $OUTPUTSTRING"
    exit 1
fi
