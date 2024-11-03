import json

read_PATH = "./lille-nodes_metadata_with-mac.json"
write_PATH = "./deployment_mapping.c"

COORDINATOR_ID = 238

# Load the JSON file
with open(read_PATH, 'r') as f:
    data = json.load(f)

# Start building the C code string
c_code = """#include <stdint.h>

#define COORDINATOR_ID {COORDINATOR_ID}

typedef struct {
    uint16_t id;
    uint8_t mac[8];
} id_mac;

const id_mac deployment_fit_iot_lab[] = {
"""

# Iterate over each node and format the MAC address
for node_id, node_data in sorted(data["nodes"].items(), key=lambda x: int(x[0])):
    #print(node_data['MAC'])
    try:
        # Convert MAC address string to hexadecimal format
        mac_str = node_data['MAC'].split('.')
        mac_hex = ', '.join(f"0x{int(octet):02x}" for octet in mac_str)
        
        # Add each entry to the C code
        c_code += f"    {{ {node_id}, {{ {mac_hex} }} }},\n"
        
    except KeyError:
        print(f"Node {node_id} has no MAC address")

# Add the terminator and close the array
c_code += "    { 0, { 0 } } // Terminator\n};\n"

# Write the generated C code to a file
with open(write_PATH, 'w') as f:
    f.write(c_code)

print(f"C code has been generated in {write_PATH}")
