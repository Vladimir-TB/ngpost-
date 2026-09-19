# Actuele projectstatus

Datum: **19 september 2026**. Fase: lokaal UI-herstel. Opleverstatus: **colorpicker hersteld en lokaal gebouwd; geen nieuwe installerrelease**.

## Gereed
- De kleurkiezer en Reset zijn weer bereikbaar via **Voorkeuren → Themakleur**, onderaan naast de taal- en thema-instellingen. Scroll zo nodig omlaag.
- De gekozen accentkleur wordt toegepast in licht en donker, automatisch opgeslagen en bij starten teruggeladen. Annuleren behoudt de bestaande kleur. Reset herstelt `#0A66C2`.
- Ook de algemene knop Opslaan bewaart `THEME_COLOR`. Lichte accentkleuren krijgen donkere selectietekst.
- Lokale app: [dist-qt6/ngPost.exe](dist-qt6/ngPost.exe). Bronversie blijft `5.1.1`; Windows-bestandsversie is nog `0.0.0.0`.
- Bronbasis: `11af4b5`. Herstel, gerichte regressiecheck en dit statusdocument horen bij de commit `Restore theme color picker in Preferences`.

## Bewezen/getest
- Qt 6.8.3/MSVC releasebuild geslaagd.
- [tests/run-colorpicker-smoke.ps1](tests/run-colorpicker-smoke.ps1) slaagt: zichtbaarheid, kleurvenster via knop, bestaande kleur laden, kiezen, annuleren, licht/donker, selectietekstcontrast en algemene configuratieopslag.
- Tweede testproces bewijst terugladen na herstart en resetten inclusief opslag. Tests linken de gebouwde productieobjecten en gebruiken uitsluitend een eigen portable-configuratie onder `artifacts/`.
- Voorkeuren visueel gecontroleerd via Qt-rendering. Dit is een offscreen UI-smokecheck, geen handmatige test van het native Windows-kleurvenster.
- Logs: [build](artifacts/colorpicker/build.log), [UI-smoke](artifacts/colorpicker/smoke.log), [herstart](artifacts/colorpicker/restart.log). [UI-beeld](artifacts/colorpicker/smoke-build/release/preferences-default.png).
- Build- en distributie-executable hebben dezelfde SHA-256: `F27FF724F4C75427505CABEE04CCE50E3DDA87AD6DDAC2A5FB95708538A336A5`.
- Oude executable bewaard in [backup](artifacts/colorpicker/backup-20260919-134701/ngPost.exe); oude SHA-256: `0ABEC02AF7448D7B86A0797EA13B14C4F77144ABC548772C3D8CB8D2A5BB6CD3`.

## Resteert / bewust uitgesteld
- Start de bijgewerkte lokale app om de kleurkeuze zelf te bekijken. Een reeds geïnstalleerde versie elders op Windows is niet bijgewerkt.
- Installers in `dist-qt6/installer/` zijn niet opnieuw gebouwd en bevatten dit herstel niet. Packaging, ondertekening en installatiecontrole vallen buiten dit UI-herstel.
- Bestaande gebruikerswijzigingen in `release.md`, `release-old.md` en de losse schermafbeelding zijn behouden. Persoonlijke configuraties zijn niet aangepast.

Projectingang en mapfuncties: [README.md](README.md).
