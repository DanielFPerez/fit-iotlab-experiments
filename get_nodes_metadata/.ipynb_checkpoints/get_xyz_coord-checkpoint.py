import subprocess
import re 
import json
import argparse

# Use argparse to handle command-line input for experiment_id
parser = argparse.ArgumentParser(description="Fetch node information from IoT-LAB.")
parser.add_argument("experiment_id", type=str, help="The ID of the IoT-LAB experiment")

# Parse command-line arguments
args = parser.parse_args()

# Use the provided experiment_id from the command line
experiment_id = args.experiment_id

# Command to retrieve node details, including coordinates
command = f"iotlab-experiment get -i {experiment_id} -n"

def execute_local_command(command):
    try:
        # Execute the command locally
        result = subprocess.run(command, shell=True, capture_output=True, text=True)
        
        # Check if the command executed successfully
        if result.returncode == 0:
            return result.stdout
        else:
            print(f"Error: Command failed with return code {result.returncode}")
            print(f"Error output: {result.stderr}")
            return None
    except Exception as e:
        print(f"Failed to execute command: {e}")
        return None

# Run the command and capture the output
output = execute_local_command(command)

if output:
    print("Command executed successfully. Output:")
    # print(output)

    try:
        nodes = json.loads(output)

        # Initialize a dictionary to store the final JSON structure
        result_json = {
            'site': '',
            'nodes': {}
        }

        # Flag to indicate if we have already extracted the site name
        site_extracted = False
        
        # Iterate through nodes and extract required info
        for node in nodes['items']:

            uid = node['uid']
            network_address = node['network_address']

            # Extract site name from the first node only
            if not site_extracted:
                site_match = re.search(r'\.(.*?)\.', network_address)
                if site_match:
                    site_name = site_match.group(1)
                    result_json['site'] = site_name  # Set the site in the result JSON
                    site_extracted = True  # Set flag to avoid extracting site again

            # Use regex to extract the number between "m3-" and "."
            node_number_match = re.search(r'm3-(\d+)\.', network_address)
            if node_number_match:
                node_number = node_number_match.group(1)
                # Add node info to the result JSON
                result_json['nodes'][node_number] = {
                    'x': float(node['x']),
                    'y': float(node['y']),
                    'z': float(node['z']),
                    'uid': uid
                }
                print(f"Node {node_number}, Coordinates (x: {node['x']}, y: {node['y']}, z: {node['z']})")
            else:
                print(f"Could not extract node number from network address: {network_address}")
        
        # Save the result as a JSON file locally
        with open(f'{result_json["site"]}-nodes_coord.json', 'w') as json_file:
            json.dump(result_json, json_file, indent=4)

        print(f"Node data saved to '{result_json['site']}-nodes_coord.json' successfully.")

    except json.JSONDecodeError:
        print("Failed to parse JSON output. Raw output:")
        print(output)


else:
    print("No output or an error occurred.")

