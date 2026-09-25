# Release freeze

**Status: ACTIVE**

## What this means

No new version may be published while this file exists.

Concretely: do not push a `v*` tag, and do not publish a GitHub Release by hand.
The release workflow refuses to run while this file is present, so a tag push
fails loudly instead of shipping a version nobody meant to ship.

## Why

The companion-plugin work (pinned submodules, the single installer wizard, the
native plugin transport) is still landing. While it is in progress:

- the plugin ABI and the packaging layout can still change between commits;
- a released version is a promise to players, and a version that installs a
  half-configured plugin set is worse than no release;
- an unreleased tree can be corrected by a commit, a released one cannot.

## What is still allowed

Everything except publishing a version:

| Action | Allowed |
| --- | --- |
| Pushing commits to `main` | yes |
| Running CI (builds, gates) | yes |
| Manually running the release workflow to test packaging | no - the guard blocks it |
| Pushing a `v*` tag | no |
| Publishing a GitHub Release | no |

If you need to exercise the packaging without publishing, run
`Tools/Packaging/New-STNModPackage.ps1` locally, or the `plugins.yml` gates.
Both are dry runs that ship nothing.

## How to lift it

1. Confirm the companion-plugin work has landed and the gates pass.
2. Delete this file in the same commit that prepares the release.
3. The release workflow runs again on the next `v*` tag.

Deleting this file is the whole switch: the guard checks for its presence, so
there is no second place to remember.

## Background

Recorded on request while adding the companion-plugin feature set. It is a
project policy, not a technical limitation: the release workflow is intact and
starts working again the moment this file is gone.