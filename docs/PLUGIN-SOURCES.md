# Companion plugin sources: durability and updating

The four companion plugins live in `plugins/` as git submodules pointing at
repositories this project does not own and cannot push to. That arrangement is
good for following upstream and bad for survival: a submodule records a commit
hash, and if the repository hosting that commit disappears, the hash points at
nothing. A submodule is not a backup.

`snapshots/plugins/<id>/` is the backup. It holds the tracked files of each
plugin at the pinned commit as ordinary content of this repository, so they
survive in its history.

| | Submodule (`plugins/`) | Snapshot (`snapshots/`) |
| --- | --- | --- |
| What it is | a pointer to a commit in someone else's repository | plain files committed here |
| What builds | yes | no |
| Survives upstream deletion | no | yes |
| What the gates check | the consumer contract | that it matches the pinned commit |

## Updating a plugin

```text
# 1. see what upstream has that this project does not
python Code/plugins/tools/plugin_snapshot.py update

# 2. re-pin the submodules to their upstream tips
python Code/plugins/tools/plugin_snapshot.py update --apply

# 3. refresh the snapshots and run the gates
python Code/plugins/tools/plugin_snapshot.py snapshot
python Code/plugins/tools/strpm_contract.py check
python Code/plugins/tools/plugin_snapshot.py verify

# 4. commit the submodule pointer and the snapshot together
```

Step 4 is the one that matters. The snapshot gate fails when a plugin is
re-pinned and `snapshots/` was not refreshed in the same commit, which is what
stops a backup from silently falling behind the thing it backs up.

`update` only reports by default. `--apply` moves the working tree and stages
the new pointer; it never commits, because re-pinning a plugin is a decision
that should be reviewed alongside what the plugin changed.

## If an upstream repository disappears

```text
python Code/plugins/tools/plugin_snapshot.py restore --plugin OStimTogether
```

This materialises the plugin's source from the snapshot. It refuses to write
into a populated directory without `--force`, so it cannot quietly overwrite a
working tree.

Two things to do afterwards, because restoring is only half the job:

1. **Decide what to do with the submodule entry.** `.gitmodules` still points
   at the dead URL, and a later `git submodule update` would replace the
   restored files with a fetch failure or, worse, with whatever now answers at
   that URL. Either remove the entry and keep the source as ordinary
   directories, or point it at wherever the project moved to.
2. **Record the fork point.** The snapshot is of one commit, not of the
   project's future. Anyone continuing the work is starting from that commit.

## What is deliberately not in the snapshot

Only tracked files. Build output, `dist/`, `build/`, `release-fomod/` and
anything else ignored upstream are excluded, which is why the whole backup is
about 1.6 MB of source rather than a repository copy.

Line endings are normalised to LF before hashing, so the same commit compared on
a Windows checkout and a Linux one agrees. Without that, `verify` would report
drift on every machine whose git settings differ.