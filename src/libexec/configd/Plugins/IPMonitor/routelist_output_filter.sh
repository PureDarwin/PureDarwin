#!/bin/sh
while IFS="" read a
do
    if [ "${a}" = "Test Complete" ]; then
	exit 0
    fi
    echo "${a}"
done
