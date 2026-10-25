# RETIRED in the v1.8.3 -> v1.8.4 port (upstream commit 0882e77d deleted this
# script). This file now only keeps the deletion visible to a repo that cannot
# remove files yet; delete it together with the deletion of
# `address_patcher.py` and `get_skyrim_addresses.py`.
#
# It fed a Fallout 4 address dump (out.txt) through every .cpp/.h in the current
# directory to rewrite POINTER_FALLOUT4 entries, which has not matched this
# codebase for years. Use Tools/Scripts/addrlib.py instead.

addresses = {}

with open('out.txt') as f:
    lines = f.readlines()
    for l in lines:
        split_string = l.split("\t")
        addresses[split_string[1][:-1]] = split_string[0]

import glob
import re

def handle_file(filename):
    if filename == 'Utils.h' or filename == "EventDispatcher.h":
        return

    with open(filename, 'r') as file :
        filedata = file.read()

    replace = {}
    pointers = [m.start() for m in re.finditer('POINTER_FALLOUT4', filedata)]
    if len(pointers) > 0:
        for p in pointers:
            start_loc = filedata.find('0x', p)
            mid_loc = filedata.find(' ', start_loc)
            end_loc = filedata.find(')', start_loc)

            addr = filedata[start_loc:mid_loc].lower()
            try:
                id = addresses[addr]
                replace[filedata[start_loc:end_loc]] = id
            except:
                print("Missing " + addr)

    #filedata.find( 'POINTER_SKYRIMSE')

    if len(replace) > 0:
        for fr in replace:
            filedata = filedata.replace(fr, replace[fr])

        with open(filename, 'w') as file:
            file.write(filedata)

# root_dir needs a trailing slash (i.e. /root/dir/)
for filename in glob.iglob('**/*.cpp', recursive=True):
    handle_file(filename)

for filename in glob.iglob('**/*.hpp', recursive=True):
    handle_file(filename)

for filename in glob.iglob('**/*.h', recursive=True):
    handle_file(filename)
