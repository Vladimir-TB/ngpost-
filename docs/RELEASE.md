# Release bouwen en broncodevoorziening

Deze handleiding beschrijft de bouwroute. Actuele opleverstatus: [STATUS.md](../STATUS.md). Publieke release-inhoud: [5.1.2](releases/5.1.2.md).

## Applicatie bouwen

Gebruik Windows x64, PowerShell 7, Visual Studio 2022 met Desktop development with C++, Qt 6.8.3 MSVC 2022 x64 (inclusief qmake/lrelease) en Inno Setup. De scripts verwachten Qt in `.qt/6.8.3/msvc2022_64` en VS Community in het standaardpad; pas de toolchainpaden aan bij een andere installatie.

1. Pak `ngPost-source-v5.1.2.zip` uit of checkout tag `v5.1.2`.
2. Voor alleen de app: open een VS x64 Developer Command Prompt, maak `build-qt6`, voer daar `<Qt-bin>\qmake.exe ..\src\ngPost.pro` en `nmake /f Makefile.Release` uit. Vertalingen zijn meegeleverd; bouw gewijzigde TS-bestanden met `lrelease` opnieuw.
3. Voor pakketten: plaats eigen rechtmatig verkregen RAR/PAR2-binaries en de oorspronkelijke Microsoft `vc_redist.x64.exe` in `dist-qt6`. Het project bevat geen proprietary broncode of gebruikslicentie daarvoor. RAR-bundeltoestemming van deze uitgever gaat niet automatisch over op andere distributeurs.
4. Voer `pwsh -File build-qt6.ps1` uit. De vorige distributie en eventuele lokale configuratie worden bewaard; nieuwe runtime/payload staat in `dist-qt6`.
5. Voer `pwsh -File build-installers.ps1 -IsccPath <pad-naar-ISCC.exe>` uit. Installer en portable komen onder `release/5.1.2-UNSIGNED`. De vaste AppId moet behouden blijven voor upgrades.
6. Controleer met de scripts onder `tests/`; zie STATUS voor hun scope. `run-installer-smoke.ps1` accepteert hetzelfde `-IsccPath`.
7. Leg de geteste bron vast. `pwsh -File installer/package-sources.ps1` archiveert HEAD en de hieronder beschreven dependencies en vernieuwt alle releasechecksums. Publiceer tag, binaries, beide bronpakketten en checksums samen.

## Exacte dependencies

De uitgeleverde Qt DLL's/plugins zijn ongewijzigde Qt 6.8.3 MSVC 2022 x64 SDK-bestanden. Gebruikte modules: qtbase (Core, Concurrent, GUI, Widgets, Network en Windows-/TLS-/beeldplugins) en qtsvg (SVG en SVG-iconen). Er zijn geen Qt Quick/WebEngine-modules in deze payload. Bronarchieven van [Qt's officiële archief](https://download.qt.io/archive/qt/6.8/6.8.3/submodules/):

| Bestand | SHA-256 |
|---|---|
| qtbase-everywhere-src-6.8.3.tar.xz | 56001b905601bb9023d399f3ba780d7fa940f3e4861e496a7c490331f49e0b80 |
| qtsvg-everywhere-src-6.8.3.tar.xz | 35eb516460f00f264eb504baa253432384351cf23fb9980a5857190e8deef438 |

De hashes zijn vergeleken met de `.sha256`-bestanden op dezelfde Qt-server. Pak de archieven uit; zij bevatten de configure-/CMake-bestanden, ingebouwde derden, licenties en buildinstructies. Qt kan met MSVC 2022/CMake/Ninja als gedeelde bibliotheken opnieuw worden gebouwd (eerst qtbase, daarna qtsvg tegen die installatie). Gebruik het SDK voor algemene ontwikkelhulpmiddelen. De applicatie blokkeert geen vervanging van compatibele Qt DLL's/plugins; behoud de mapindeling. Zie ook [Qt bouwen vanuit bron](https://doc.qt.io/qt-6.8/windows-building.html).

`par2.exe` is byte-identiek aan `par2cmdline-turbo-1.3.0-win-x64.zip` uit de [officiële release](https://github.com/animetosho/par2cmdline-turbo/releases/tag/v1.3.0). SHA-256 executable: `f582c368a07d4b0bbbefc0b592bac79077e16fd8bb1dd3c19f04bf18c444c176`. Sourcecommit: `f220e1f1a74796006ca01e717e411a11e7b69a07` (tag v1.3.0). Het bronpakket bevat ook ingebouwde ParPar-code en `.github/workflows/build-release.yml`; de officiële Windows-route gebruikt `msbuild -property:PlatformToolset=ClangCL -property:Configuration=Release -property:Platform=x64 par2cmdline.sln`.

## Notices en bronpakketten opnieuw verzamelen

Download de twee Qt-archieven naar `artifacts/release-compliance` en pak ze daar uit. Clone de par2-tag naar `artifacts/release-compliance/par2-source`; download [RAR EULA](https://www.rarlab.com/license.htm) naar `rar-license.html` in diezelfde werkmap. `python installer/prepare-notices.py` verzamelt de oorspronkelijke licenties/attributions naar `installer/notices`. De notices zijn al opgenomen in de repository, dus voor een gewone appbuild is deze stap niet nodig.

`installer/package-sources.ps1` verifieert de Qt-hashes, par2-commit en binaryhash vóór archiveren. Het applicatiearchief komt uitsluitend uit Git; lokale configuraties, gebruikerswijzigingen, binaries, logs en buildstaging worden niet meegestuurd. De bronpakketten worden als download naast de binaries op dezelfde GitHub-release aangeboden.

Microsoft runtime: de originele ondertekende redistributable uit de Visual Studio-installatie; [distributievoorwaarden](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files). De huidige payload bevat geen losse `dxcompiler.dll`/`dxil.dll`.
