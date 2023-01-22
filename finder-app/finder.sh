#!/bin/bash
filesdir=$1
searchstr=$2

#If condition to test check if any arguments are missing
if [ $# -eq 2 ]; then
	:
else 
	echo "Arguments are missing"
	exit 1
fi

#If condition to test check if the directory entered is valid
if [ -d ${filesdir} ]; then
	:
else 
	echo "Invalid Directory"
	exit 2
fi

cd ${filesdir} #Change directory to the one specified by the user

X=$(grep -r -l ${searchstr} * | wc -l) #Find the unique number of files
Y=$(grep -r ${searchstr} * | wc -l) #Find the number of matching lines
echo "The number of files are ${X} and the number of matching lines are ${Y}"
