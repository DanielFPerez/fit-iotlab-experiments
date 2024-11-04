#!/bin/bash

# Redirect all nodes serial output to a file
readonly OUTFILE="${HOME}/.iot-lab/${EXP_ID}/serial_output.txt"
readonly ERRFILE="${HOME}/.iot-lab/${EXP_ID}/serial_error.txt"

echo "Launch serial_aggregator with exp_id==${EXP_ID}" >&2
serial_aggregator -i ${EXP_ID} 2> ${ERRFILE} 1> ${OUTFILE}