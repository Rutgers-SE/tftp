#!/usr/bin/env bash


./Tftp localhost 10001 -c get files/a &
./Tftp localhost 10001 -c get files/b &
./Tftp localhost 10001 -c get files/c &
./Tftp localhost 10001 -c get files/d &

echo "waiting"

wait
