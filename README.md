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

## Releases

Pushing a tag such as `r1862-1` builds macOS (universal app), Linux (x86_64,
arm64) and Windows (x86_64) archives and publishes them as a GitHub release
(`.github/workflows/release.yml`). The Actions tab can also run it by hand,
which builds the archives without releasing them.

The macOS app is signed with a Developer ID and notarized when the repository
has these secrets, and ad-hoc signed otherwise:

- `MACOS_CERTIFICATE` (Developer ID Application `.p12`, base64),
  `MACOS_CERTIFICATE_PWD`, `MACOS_TEAM_ID`
- notarization, either `MACOS_NOTARIZE_APPLE_ID` + `MACOS_NOTARIZE_PASSWORD`
  (app-specific password), or an App Store Connect API key:
  `MACOS_NOTARIZE_API_KEY` (`.p8`, base64), `MACOS_NOTARIZE_API_KEY_ID`,
  `MACOS_NOTARIZE_API_ISSUER`

## License

Previous is free software under the GNU General Public License, version 2 or
(at your option) any later version, and so is this fork. See `gpl.txt`.
