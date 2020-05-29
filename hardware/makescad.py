# makescad.py v0.2
# Alessandro Gangemi

import sys
import os
import re

if (len(sys.argv) == 1):
    print("Usage: makescad.py <group> <src> <output_dir>")
    exit()

group = sys.argv[1]
fn = sys.argv[2]
output_path = sys.argv[3]

# Read lines
lines = []
orig_file = []
with open(fn) as file:
    line = file.readline()
    while line:
        if "GROUP" in line:
            lines.append(line.rstrip())
        else:
            orig_file.append(line)
        line = file.readline()

# Wipe slashes
cleaned_lines = []
for line in lines:
    cleaned_lines.append(line.replace('/', ''))

# Filter by group
grouped = list(filter(lambda x: (group in x.split()[2]), cleaned_lines))

# Get funcs
funcs = []
for line in grouped:
    funcs.append(line.split()[0])

# Get names
names = []
for line in grouped:
    s = re.search("NAME(\\s+|=)\"(\\w+)\"", line)
    if (s == None):
        names.append(line.split()[0].replace("();", ''))
    else:
        names.append(s.group(2))
    

# Get quantities
quants = []
for line in grouped:
    s = line.split("QTY")
    if (len(s) == 1):
        quant = 1
    else:
        quant = s[1]
    quants.append(quant)

# Merge name, func, and quantity
combined = zip(names, funcs, quants)

# Open directory for merging
if (not os.path.exists(output_path)):
    try:
        os.mkdir(output_path)
    except OSError:
        print("Failed to create directory %s failed" % output_path)
    else:
        print("Created output directory")

# Create output files
for item in combined:
    filename = output_path + '/' + item[0]
    func = item[1]
    quantity = item[2]

    for n in range(int(quantity)):
        f = open(filename + str(n) + ".scad", 'w')
        for line in orig_file:
            f.write(line)
        f.write("\n" + func)