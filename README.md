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

`.github/workflows/release.yml` builds macOS (universal app), Linux (x86_64,
arm64) and Windows (x86_64) archives. To publish a release:

- Actions tab > "Build and release" > "Run workflow": pick the branch and
  signing mode, enter a new tag such as `r1862-1` in "release_tag", run. The
  workflow builds everything, creates the tag and publishes the release.
- Or push a tag (`git tag r1862-1 && git push origin r1862-1`).

A manual run with "release_tag" left empty only builds; the archives are
attached to the run (Summary page, "Artifacts").

The macOS signing mode is the "signing" choice of a manual run, else the
repository variable `MACOS_SIGNING`, else `ad-hoc`. The choice defaults to
"repository setting", so set `MACOS_SIGNING` once (Settings > Secrets and
variables > Actions > Variables) and every run, manual or tag, uses it:

- `ad-hoc` (default): no certificate needed. macOS asks for confirmation the
  first time the app is opened.
- `developer-signing`: Developer ID signature and Apple notarization, so the
  app opens without warnings. Needs these secrets:
  `MACOS_CERTIFICATE` (Developer ID Application `.p12`, base64),
  `MACOS_CERTIFICATE_PWD`, `MACOS_TEAM_ID`, and for notarization either
  `MACOS_NOTARIZE_API_KEY` (App Store Connect `.p8`, base64),
  `MACOS_NOTARIZE_API_KEY_ID`, `MACOS_NOTARIZE_API_ISSUER`, or
  `MACOS_NOTARIZE_APPLE_ID` + `MACOS_NOTARIZE_PASSWORD` (app-specific password).

The Mac App Store isn't an option for this project: its terms conflict with
the GPL.

## License

Previous is free software under the GNU General Public License, version 2 or
(at your option) any later version, and so is this fork. See `gpl.txt`.
