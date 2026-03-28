# ngPost Qt6 port

Minimal Qt 6 port van ngPost.

## Build (Windows)
- Installeer Qt 6.8.x + MSVC 2022
- Run `build-qt6.ps1`

## Installer (Windows)
- Installeer Inno Setup 6
- Optioneel: zet `ISCC_PATH` naar het pad van `ISCC.exe`
- Run `build-installers.ps1`

## Notities
- Build output staat in `build-qt6/` en `dist-qt6/` (niet in git)
- Release notes staan in `release.md` en `release_notes*.txt`