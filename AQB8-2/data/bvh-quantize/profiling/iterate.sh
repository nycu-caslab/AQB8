#!/bin/bash

if [ "$#" -ne 7 ]; then
  echo "usage: $0 SCENE T_TRV_INT-start T_TRV_INT-incr T_TRV_INT-end T_SWITCH-start T_SWITCH-incr T_SWITCH-end"
  exit
fi

for i in $(seq "$2" "$3" "$4"); do
  for j in $(seq "$5" "$6" "$7"); do
    OUTPUT=$(SCENE=$1 T_TRV_INT=${i} T_SWITCH=${j} make quantized.target 2>&1)
    CLOCK=$(grep "task-clock" <<< "${OUTPUT}" | awk '{print $1}' | sed 's/,//g')
    L1_LOADS=$(grep "L1-dcache-loads" <<< "${OUTPUT}" | awk '{print $1}' | sed 's/,//g')
    L1_MISSES=$(grep "L1-dcache-load-misses" <<< "${OUTPUT}" | awk '{print $1}' | sed 's/,//g')
    LLC_LOADS=$(grep "LLC-loads" <<< "${OUTPUT}" | awk '{print $1}' | sed 's/,//g')
    LLC_MISSES=$(grep "LLC-load-misses" <<< "${OUTPUT}" | awk '{print $1}' | sed 's/,//g')
    echo "${i} ${j} ${CLOCK} ${L1_LOADS} ${L1_MISSES} ${LLC_LOADS} ${LLC_MISSES}"
  done
done
