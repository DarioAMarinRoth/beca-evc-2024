#!/bin/bash

pkill -x socat 2>/dev/null
pkill -x chacras_slave 2>/dev/null

socat -d -d pty,raw,echo=0,link=/tmp/ttyV0 pty,raw,echo=0,link=/tmp/ttyV1 &
sleep 1

./chacras_slave /tmp/ttyV1 115200 &
sleep 1

echo "Simulation initiated"
echo "To end the simulation, execute: ./kill_sim.sh"