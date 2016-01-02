#!/bin/sh

HOST=$1
COUNT=$2

if [ $COUNT -eq 0 ]
then
    COUNT=100
    echo "Using default count of $COUNT"
fi

echo "Pinging $HOST with $COUNT packets..."
PINGOUT=`ping -c $COUNT $HOST`

SENDCNT=`echo "$PINGOUT" | grep -e '^[0-9]{1,} packet'` # | awk -F' ' {'print $1'}`
RECVCNT=`echo $PINGOUT | awk -F' ' {'print $4'}`

echo -e "Sent:\t$SENDCNT Received:\t$RECVCNT"

#3 packets transmitted, 3 received, 0% packet loss, time 1998ms