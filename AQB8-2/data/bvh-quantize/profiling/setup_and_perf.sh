#!/bin/bash

# create ctl.fifo
test -p ctl.fifo && unlink ctl.fifo
mkfifo ctl.fifo
exec {CTL_FD}<>ctl.fifo
export CTL_FD=${CTL_FD}

# create ack.fifo
test -p ack.fifo && unlink ack.fifo
mkfifo ack.fifo
exec {ACK_FD}<>ack.fifo
export ACK_FD=${ACK_FD}

# start profiling
perf stat --delay=-1 -d --control=fd:${CTL_FD},${ACK_FD} -- "./$1" "$2" "$3" "$4"
