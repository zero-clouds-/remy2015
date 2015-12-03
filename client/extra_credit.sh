#!/bin/bash

# args
arg_count=8

if [[ $# -ne $arg_count ]]; then
    echo $#
    echo "Usage: $0 -h <hostname> -p <port> -n <number of sides> -l <length of sides>"
    exit 1
fi

args=( "$@" )
counter=0
while [[ $counter -lt $arg_count ]]; do
    if [ ${args[$counter]} == "-h" ]; then
        hostname=${args[$counter+1]}
    fi
    if [ ${args[$counter]} == "-p" ]; then
        port=${args[$counter+1]}
    fi
    if [ ${args[$counter]} == "-n" ]; then
        sides=${args[$counter+1]}
    fi
    if [ ${args[$counter]} == "-l" ]; then
        side_length=${args[$counter+1]}
    fi
    counter+=2
done

host=`getent hosts $hostname | awk '{ print $1 }'`
echo $host


# requests
CONNECT=0
IMAGE=2
GPS=4
dGPS=8
LASERS=16
MOVE=32
TURN=64
STOP=128
QUIT=255
ERROR_CLIENT=256
ERRO_ROBOT=512

# header
header=(0 0 0 0 0 0 0)

# header indices
version=0
password=1
request=2
data=3
offset=4
total=5
payload=6

socksend()
{
    SENDME="$1"
    echo "sending: $SENDME"
    echo -ne "$SENDME" >&5 &
}

sockread()
{
    LENGTH="$1"
    RETURN=`dd bs =$1 count=1 <&5 2> /dev/null`
}

join()
{
    local IFS=""
    RETURN=`echo "$*"`
}

echo "trying to open socket"
if ! exec 5<> /dev/udp/$host/$port; then
    echo "`basename $0`: unable to connec to $host:$port"
    exit 1
fi
echo "socket is open"

join ${header[@]}
socksend $RETURN

sockread 28
echo "RETURN: $RETURN"
