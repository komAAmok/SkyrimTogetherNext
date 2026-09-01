# IDA kit for completing the 1.5.97 address mapping
#
# Files:
#   1_apply_st_map.py   - run on the 1.5.97 IDB: labels the 3603 already-known
#                         functions (from the ST map) and prints the 93-target
#                         checklist with their 1.6.1170 reference addresses.
#   2_apply_ae_labels.py- run on a 1.6.1170 IDB (optional but recommended):
#                         labels every AE library function so the 93 targets
#                         can be located by id instantly.
#   3_export_targets.py - run on the 1.5.97 IDB after identification: exports
#                         functions you renamed to STtarget_ae<id> into an
#                         overrides file for gen_se_map_from_history.py.
#
# Workflow doc: Tools/missing_1_5_97_ids.txt (the 93-target manifest).
