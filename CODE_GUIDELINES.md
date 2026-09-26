# Code Guidelines

## Language

We are using C++20, any C++20 feature supported by vs2019 is allowed.

Please try to use templates responsibly, we don't want compilation times to explode and to deal with bloated binaries.

Try to follow SRP as much as possible, a huge class containing tons of functionnalities is not better than many small components, it's easier to re-use them and to extend.

## Naming

### Variables

The first letter is lower case, other words must start with an upper case : ``someVariableName``.

Function arguments must be prefixed with an ``a`` for 'argument' : ``aFunctionArgument``.

Const variables must be prefixed with a ``c`` for 'const' : ``const int cSomeInt;``

Pointers must be prefixed with a ``p`` for 'pointer' : ``int* pSomePointer``.

The rules above must be used together for example ``void SomeFunc(const int* acpSomeArgument)``.

Static variables must be prefixed with ``s_`` : ``static int s_someInt;``.

Global variables must be prefixed with ``g_`` : ``extern int g_someGlobalInt;``.

### Classes

Class names must start with an upper case : ``class SomeClass``.

Class attributes must be prefixed with ``m_`` and must use the same rules as variables : ``int* m_pSomeMemberPointer;``.

### Functions

All functions must start with an upper case : ``void SomeFunc();``.

## Generalities

Names must be self explanatory, ``size_t a;`` is not acceptable, ``size_t incomingPacketCount;`` is good.

``auto`` is allowed when dealing with long names, it is not accepted for primitive types as we don't want the compiler to give us a signed int when we are using it as unsigned.

Don't use java style blocks, a ``{`` needs to be on a new line.

Don't use exceptions, don't use STL code that can throw, use the nothrow version if available or the unsafe version.

## Commit Naming
Preface commit messages with one of the following tags:
* `feat:` for new features
* `tweak:` for tweaks to an existing system
* `fix:` for bug fixes and crash fixes
* `refactor:` for refactoring

## Documentation is part of the change

A change is not finished when the code compiles. **Every version change must update
the documentation in the same commit**, and a release must not be tagged until it
has. This is not a style preference: the documentation in this repository is what
tells a player which build to install, which game versions work, and how far 1.5.x
coverage actually reaches. Every one of those is a claim about the code, and a claim
that drifts is worse than no claim at all, because it is believed.

What "the documentation" means for a given change:

| The change touches | Update, in the same commit |
| --- | --- |
| Anything a player can observe | `CHANGELOG.md` - a new `## <version>` section (or the unreleased heading, before a tag) |
| A released version | `README.md` **and** `README_EN.md` version tables, kept in step with each other |
| Which game versions work, or how far the mapping reaches | the 1.5.x note in both READMEs, plus `Tools/missing_1_5_97_ids.txt` when the id set changes |
| Address-library coverage or the recovery tooling | `Tools/ida/README.md` status line and `docs/LAN-RADMIN-GUIDE.md` |
| The plugin layer | `docs/COMPANION-PLUGINS.md`, `docs/PLUGIN-SOURCES.md` |
| Packaging, FOMOD, or the install flow | `docs/RELEASE-AND-MO2.md` |

Two rules that follow from this, and that reviewers should hold the line on:

- **Numbers in prose must come from the tool that owns them, not from memory.** The
  1.5.x coverage figure is produced by `gen_ae_to_se_map.py::collect_codebase_ids`;
  run it rather than copying a number from an earlier paragraph. That number has
  already drifted once because it was recounted by hand.
- **A claim that a gate can check should be checked by a gate.** Where documentation
  states a value that also exists in a machine-readable file - the coverage figures,
  the id manifest's own count, whether every released tag has a changelog section -
  `Tools/Scripts/check_docs.py` is where that belongs. Prose that nothing verifies
  is prose that will eventually be wrong.

Run `python Tools/Scripts/check_docs.py` before tagging. It is part of the release
checklist, and it is wired into CI.