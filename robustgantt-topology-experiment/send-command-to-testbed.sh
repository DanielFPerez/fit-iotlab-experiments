#!/bin/bash

# Activate Python virtual environment
source /usr/local/share/virtualenvs/fittestbed/bin/activate

cd /home/daniel/Documents/00_NI-DNA/MLCarrSched_work/fit-iotlab-experiments/robustgantt-topology-experiment

# Submit the experiment and wait for it to start running
iotlab-experiment submit -n robgant -d 60 -l 230,archi=m3:at86rf231+site=lille -s lille,script=serial_script.sh

# Wait for the experiment and capture only the last line of output
status=$(iotlab-experiment wait | tail -n 1)

# Check if the experiment is running
if [[ "$status" == *"Running"* ]]; then
    # Flash the udp-server
    iotlab-node --flash udp-server.iotlab -l lille,m3,238

    # Wait for 15 seconds before proceeding
    sleep 15

    # Flash the udp-client
    iotlab-node --flash udp-client.iotlab -e lille,m3,238
else
    echo "Experiment is not in the 'Running' state. Exiting."
fi

