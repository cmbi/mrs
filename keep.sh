#!/bin/sh

while true; do
  mrs server start -p /var/run/mrs.pid --no-daemon
done
