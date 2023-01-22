#!/bin/bash

writefile=$1
writestr=$2

#If condition to test check if any arguments are missing
if [ $# -eq 2 ]; then
	:
else 
	echo "Arguments are missing"
	exit 1
fi

T=$(dirname ${writefile}) #To store the directory where files needs to be created
mkdir -p ${T} #Make the directory if not present

echo ${writestr} > ${writefile} #Create the file and copy the contents entered by user in the file
