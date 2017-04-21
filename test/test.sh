#!/usr/bin/env bash


tftp localhost 10001 -c get files/a &
tftp localhost 10001 -c get files/b &
tftp localhost 10001 -c get files/c &
tftp localhost 10001 -c get files/d &

echo "waiting"

wait
