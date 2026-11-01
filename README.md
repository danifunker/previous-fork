# Previous (fork with automation support)

This is a fork of [Previous](https://sourceforge.net/projects/previous/), the
NeXT computer emulator, which is developed in Subversion at
`svn://svn.code.sf.net/p/previous/code/trunk`. Upstream's own documentation is
in `readme.previous.txt`.

This fork adds features for running Previous unattended, e.g. for automated
tests and CI. The changes are being offered upstream; until they are merged,
they are kept here.

## Changes in this fork

Modified by Dani Sarfati, November 2026, based on Previous SVN r1862. Ported
from Hatari 2.6.1 where noted.

- `src/options.c` (new, port of Hatari's `options.c`): command line options.
- `src/control.c` (new, port of Hatari's `control.c`): `--cmd-fifo` and
  `--control-socket` remote control.
- `src/scc.c`: serial ports connected to host files (`--scc-a-in/out`,
  `--scc-b-in/out`).
- `src/main.c`, `src/change.c`, `src/dialog.c`, `src/gui-sdl/dlgAlert.c`,
  `src/configuration.c`, `src/shortcut.c`, headers and CMake files: hook-up,
  `--dialogs off`, exit values, `[Serial]` configuration section.
- `automation.howto.txt` (new): how to use it.

## Branches

- `upstream`: unmodified snapshots of Previous trunk.
- `main`: `upstream` plus the changes above.

To follow upstream, add a new snapshot to `upstream` and rebase `main` on it.

## License

Previous is free software under the GNU General Public License, version 2 or
(at your option) any later version, and so is this fork. See `gpl.txt`.
