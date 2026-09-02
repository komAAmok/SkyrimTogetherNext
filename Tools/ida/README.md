# 1.5.97 address-recovery kit

## Current tool: `match_capstone.py` (no IDA required)

Recovers the remaining 1.5.97 RVAs for AE address-library ids entirely in
pure Python (capstone + numpy) -- no IDA, no license locks, no giant IDB:

* **function targets** -- normalized instruction-token Dice similarity (address
  operands become placeholders), candidate pool = union of four bracket
  interpolations (full/code anchors x id-order/rva-order), plus mapped-callee
  and shared-string bonuses. Every accepted result was additionally verified
  by side-by-side disassembly of the 1.6.1170 and 1.5.97 versions.
* **full-.text scan** (`/tmp/scan_fulltext.py` pattern) -- for targets with no
  reliable bracket, scans all ~190k 1.5.97 function starts against the target
  with a size prefilter and a shared token cache (~2 min for all targets).
* **data targets** -- rip-relative xref ordering through already-mapped
  containing functions (used where possible; several data statics remain
  unmapped by design: no stable cross-version signal).

Run (from the repo root):

    python3 Tools/ida/match_capstone.py \
        --ae-exe "<1.6.1170 SkyrimSE.exe>" \
        --se-exe "<1.5.97 SkyrimSE.exe>" \
        --ae-bin  /tmp/addrlib/SKSE/Plugins/versionlib-1-6-1170-0.bin \
        --se-bin  /tmp/addrlib/SKSE/Plugins/version-1-5-97-0.bin \
        --map     GameFiles/Skyrim/SKSE/Plugins/versionlib-ae-to-se-1-5-97-0.map \
        --missing Tools/missing_1_5_97_ids.txt \
        --out-overrides Tools/ida/st_overrides.txt \
        --out-report    Tools/ida/st_capstone_report.json

Feed the result into the map generator:

    python3 Tools/Scripts/gen_se_map_from_history.py \
        --se-bin  /tmp/addrlib/SKSE/Plugins/version-1-5-97-0.bin \
        --overrides Tools/ida/st_overrides.txt \
        --se-bins-dir /tmp/addrlib/SKSE/Plugins \
        --out GameFiles/Skyrim/SKSE/Plugins/versionlib-ae-to-se-1-5-97-0.map

Status: 3033/3058 codebase ids mapped (99.2%); all 10 1.5.x maps regenerate
and validate against version-1-5-97-0.bin (3667/3667).

## Legacy IDA kit (superseded)

`1_apply_st_map.py` / `2_apply_ae_labels.py` / `3_export_targets.py` were the
IDA-Pro flow used before `match_capstone.py`; keep them only as reference for
manually resolving the last 25 ids (near-twin siblings, tiny bodies, data
statics listed in Tools/missing_1_5_97_ids.txt).
