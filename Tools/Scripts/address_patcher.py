# RETIRED in the v1.8.3 -> v1.8.4 port (upstream commit 0882e77d deleted this
# script). This file now only keeps the deletion visible to a repo that cannot
# remove files yet; delete it together with the deletion of
# `addrlib_conv.py` and `get_skyrim_addresses.py`.
#
# It rewrote every POINTER_SKYRIMSE address in Code/client/ from a hand-kept
# new_addresses.txt, by plain whole-file string replacement, with no check that
# a new address actually covers the old one - and the framework now resolves
# everything through Address Library ids, so the file has nothing left to do.
# Use Tools/Scripts/addrlib.py instead.

import os
from pathlib import Path

# run this script from Tools/Scripts/, and put the 'new_addresses.txt' file there
rootdir = Path(__file__).parent.parent.parent.joinpath("Code").joinpath("client")

# collect all paths in the Client/ folder
paths = []

for subdir, dirs, files in os.walk(rootdir):
	for file in files:
		if ".h" in file or ".cpp" in file:
			paths.append(os.path.join(subdir, file))

with open('new_addresses.txt') as new_addresses:
	for address_pair in new_addresses:
		address_pair = address_pair.strip().split(',')

		is_address_rewritten = False

		for path in paths:
			with open(path, 'r') as source_file:
				file_data = source_file.read()

			if address_pair[0] in file_data:
				print("found address " + address_pair[0])

				file_data = file_data.replace(address_pair[0], address_pair[1])

				with open(path, 'w') as source_file:
					source_file.write(file_data)

				is_address_rewritten = True
				# don't break cause the address might be in multiple places
				# it obviously shouldnt be, but it be like that rn
				#break

		if not is_address_rewritten:
			print("Address was not found and has not been rewritten! " + address_pair[0])
			continue


print("Done patching!")
