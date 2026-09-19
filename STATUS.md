# Actuele projectstatus

Datum: **19 september 2026**. Fase: UI-thema gereed; vervolg op GitHub-feedback onderzocht. Opleverstatus: **hele UI kleurt mee, lokaal gebouwd en getest; issueplan gereed, uitvoering nog niet gestart; geen nieuwe installerrelease**.

## Gereed
- De kleurkiezer en Reset zijn weer bereikbaar via **Voorkeuren → Themakleur**, onderaan naast de taal- en thema-instellingen. Scroll zo nodig omlaag.
- De gekozen kleur bepaalt het volledige lichte of donkere thema: achtergronden, panelen, invoervelden, knoppen, randen, menu's, Voorkeuren, serverinstellingen en hulpvensters. De vaste blauw/turquoise/groene decoratie is vervangen door tinten van de gekozen kleur. Grijs, zwart en wit geven een neutraal thema.
- De kleur wordt automatisch opgeslagen en bij starten teruggeladen. Annuleren behoudt de bestaande kleur. Reset herstelt het blauwe thema (`#0A66C2`).
- Ook de algemene knop Opslaan bewaart `THEME_COLOR`. Tekst, geselecteerde knoppen en plus/min-symbolen passen hun contrast aan. Echte fout- en postingstatussen behouden hun betekenis.
- Lokale app: [dist-qt6/ngPost.exe](dist-qt6/ngPost.exe). Bronversie blijft `5.1.1`; Windows-bestandsversie is nog `0.0.0.0`.
- Bronbasis: colorpicker-herstel `0ddd5ae`. De uitbreiding naar de gehele UI, regressiecontrole en deze status horen bij de commit `Apply selected theme color throughout the UI`.

## Bewezen/getest
- Qt 6.8.3/MSVC releasebuild geslaagd.
- [tests/run-colorpicker-smoke.ps1](tests/run-colorpicker-smoke.ps1) slaagt: zichtbaarheid, kleurvenster via knop, bestaande kleur laden, kiezen, annuleren, licht/donker, selectietekstcontrast en algemene configuratieopslag.
- Acht kleuren (paars, rood, groen, blauw, grijs, zwart, wit en geel), elk in licht en donker, slagen: achtergronden, invoervelden, knoppen en randen volgen dezelfde tint; gemeten paletcontrast voor gewone tekst, invoer, secundaire tekst, links en selectie is minimaal 4,5:1.
- Tweede testproces bewijst terugladen na herstart en resetten inclusief opslag. Tests linken de gebouwde productieobjecten en gebruiken uitsluitend een eigen portable-configuratie onder `artifacts/`.
- Hoofdscherm, Snel posten, Automatisch posten en Voorkeuren visueel gecontroleerd met paarse/donkere en groene/lichte Qt-renderingen. Dit is een offscreen UI-smokecheck, geen handmatige test van het native Windows-kleurvenster.
- Logs: [build](artifacts/colorpicker/build.log), [UI-smoke](artifacts/colorpicker/smoke.log), [herstart](artifacts/colorpicker/restart.log). [UI-beeld](artifacts/colorpicker/smoke-build/release/preferences-default.png).
- Voorbeelden: [paars/donker](artifacts/colorpicker/smoke-build/release/AD37C9-dark-overviewNavButton.png), [groen/licht](artifacts/colorpicker/smoke-build/release/20C060-light-autoNavButton.png).
- Build- en distributie-executable hebben dezelfde SHA-256: `57403355E8DDF0000BDD6B13A3849FE32102747C6852E805D6D83C441A2BC0E1`.
- Vorige executable bewaard in [backup](artifacts/colorpicker/backup-20260919-135618/ngPost.exe); oude SHA-256: `F27FF724F4C75427505CABEE04CCE50E3DDA87AD6DDAC2A5FB95708538A336A5`.

## Resteert / bewust uitgesteld
- [Plan voor issues 1–3](docs/ISSUES-PLAN.md) is gereed op basis van actuele issues/reacties en brononderzoek op `3e82949`. Vier concrete verzoeken geïdentificeerd, inclusief Alle sessies starten uit de reacties. Eerstvolgende voorgestelde stap: #2 met 500 mappen reproduceren en meten. Nog geen performancebenchmark, issuefix of nieuwe installer uitgevoerd; geen berichten op GitHub geplaatst.
- Start de bijgewerkte lokale app om de kleurkeuze zelf te bekijken. Een reeds geïnstalleerde versie elders op Windows is niet bijgewerkt.
- Installers in `dist-qt6/installer/` zijn niet opnieuw gebouwd en bevatten dit herstel niet. Packaging, ondertekening en installatiecontrole vallen buiten dit UI-herstel.
- Bestaande gebruikerswijzigingen in `release.md`, `release-old.md` en de losse schermafbeelding zijn behouden. Persoonlijke configuraties zijn niet aangepast.

Projectingang en mapfuncties: [README.md](README.md).
