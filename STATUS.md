# Actuele projectstatus

Datum: **19 september 2026**. Fase: **lokale releasekandidaat 5.1.2-rc1 gereed**. Opleverstatus: **installer en portable gebouwd en lokaal gecontroleerd; INTERN / UNSIGNED, niet gepubliceerd**.

## Gereed

- Het volledige lichte/donkere UI-thema volgt de kleurkiezer onder **Voorkeuren → Themakleur**, inclusief opslag, herstart en Reset.
- Mapscans en validatie gebeuren buiten de GUI-thread. Sessies worden stapsgewijs aangemaakt met voortgang en annuleren; de gekozen tab blijft staan. Dubbele initialisatie is verwijderd.
- **Alle sessies starten** zet voorbereide sessies in tabvolgorde in de bestaande wachtrij. Actieve, ingeplande, lege en voltooide sessies worden niet opnieuw gestart. Eén uploadjob tegelijk; de bestaande instelling PREPARE_PACKING blijft werken.
- NZB-conflicten worden vooraf behandeld: overslaan, overschrijven, unieke naam of batch annuleren, inclusief toepassen op alle conflicten. Gereserveerde doelpaden kunnen niet door een andere sessie worden overschreven.
- **Kopieer** naast NZB-bestandsnaam, archiefnaam en wachtwoord. Deze velden staan onder **Geavanceerd tonen**. De kopie gebruikt de juiste sessiewaarde, ook na afronden; NZB zonder lokaal mappad. Expliciete wachtwoorden voor al ingepakte bestanden blijven ondersteund.
- Threads en externe hulpmiddelen worden asynchroon gestopt. Mislukt starten van een hulpmiddel beëindigt de job; een niet-reagerend hulpmiddel wordt na drie seconden beëindigd zonder de GUI te blokkeren. TMP_RAM-mapgroottes worden in een worker berekend.
- Zichtbare melding boven de werkruimte: de gebruiker is verantwoordelijk voor de upload en moet verspreidingsrechten hebben. Nederlandse/Engelse installervoorwaarden zijn daarop aangepast zonder volledige aansprakelijkheidsvrijstelling te beloven.
- Installer: standaard Engels, Nederlands selecteerbaar, vorige taal behouden. Een expliciete portable-doelmap wordt gerespecteerd; een bestaande portable installatie behoudt die modus bij een upgrade.
- Packaging bewaart de vorige distributie in `artifacts/distribution-backups/`. De portable-ZIP bevat geen persoonlijke configuratie. Technische uitvoer blijft onder `artifacts/`; installer en ZIP staan onder `release/`.

## Resultaat

- [Eindproducten en uitleg](release/5.1.2-rc1-INTERN-UNSIGNED/00%20-%20START%20HIER.md).
- [Installer](release/5.1.2-rc1-INTERN-UNSIGNED/ngPost-setup-v5.1.2-rc1-UNSIGNED.exe).
- [Portable ZIP](release/5.1.2-rc1-INTERN-UNSIGNED/ngPost-portable-v5.1.2-rc1-UNSIGNED.zip).
- [Direct bijgewerkte portable app](dist-qt6/ngPost.exe). Een reeds draaiend proces moet opnieuw worden gestart om deze wijzigingen te zien.
- Appversie `5.1.2-rc1`; Windows-bestandsversie `5.1.2.0`. Productiebuild, lokale portable en geteste pakketpayload hebben dezelfde executable.
- SHA-256 executable: `896A636D0AFFC39E512DED92AB45D3B4182EFDC65CB0B14AED95C45D266B43A2`.
- [SHA-256 van installer en ZIP](release/5.1.2-rc1-INTERN-UNSIGNED/SHA256SUMS.txt).
- Commitbasis vóór uitvoering: `f1eeba5`. Implementatie, tests en deze status worden samen vastgelegd in de afgebakende implementatiecommit; geen push of GitHub-bericht uitgevoerd.

## Bewezen/getest

| Controle | Resultaat / bewijs |
|---|---|
| Qt 6.8.3 / MSVC releasebuild | Geslaagd; [buildlog](artifacts/issue-validation/release-build.log). |
| 10 / 100 / 500 mappen | Alle sessies precies eenmaal, huidige tab behouden, annuleren binnen één seconde. 500 mappen: **23,65 s**, grootste GUI-pauze **95 ms**, p95 **67 ms**. [500-log](artifacts/issue-validation/result-500.log). |
| Nulmeting | Oude route: 100 mappen circa 11 s GUI-blokkade; 500 mappen na meer dan zeven minuten nog vast in GUI, gestopt. [Afgebroken nulmeting](artifacts/issue-validation/baseline-500-aborted.txt). |
| Wachtrij en jobafronding | 5 en 100 sessies tegen lokale NNTP-testserver; volgorde, NZB-segmenten, dubbelklikken, vier conflictkeuzes, dubbele doelen, ontbrekende bron, stoppen/herstarten. PREPARE_PACKING zowel uit als aan. Grootste gemeten GUI-pauze bij deze wachtrijen 45 ms. [Uit](artifacts/session-workflow/result-prepare-false.log), [aan](artifacts/session-workflow/result-prepare-true.log). |
| Kopiëren en externe processen | Unicode, spaties, voorloopnullen, vast/sessiewachtwoord, metadata na afronding, echte RAR-compressie, onstartbaar hulpmiddel en annuleren van een vastlopend hulpmiddel. |
| Thema | Acht kleuren in licht/donker; tekstcontrast minimaal 4,5:1; kiezen, annuleren, opslaan, herstart en Reset. [Smoke](artifacts/colorpicker/smoke.log), [herstart](artifacts/colorpicker/restart.log). |
| Installer en portable | Inno 7.1.0; verse Engelse setup, expliciet Nederlands, bewaarde taal, normale/portable modus, doelmap, configuratiebehoud, identieke executable en portable starten zonder Qt op PATH. [Bewijs](artifacts/installer-smoke/20260919-145425/result.txt). |

Herhalen met PowerShell 7: `tests/run-colorpicker-smoke.ps1` bouwt eerst de productieobjecten; daarna `tests/run-issue-validation.ps1` en `tests/run-session-workflow.ps1`. Packaging: `build-qt6.ps1`, `build-installers.ps1`, daarna `tests/run-installer-smoke.ps1`. De installercontrole gebruikt dezelfde bron/payload met een aparte testidentiteit en ruimt de testregistratie op, zodat de bestaande gebruikersinstallatie behouden blijft.

De aanwezige Inno CLI op `D:\Dashboard-wdw\artifacts\tools\inno-setup-7.1.0-x64\ISCC.exe` is gebruikt. Een compiler kan ook expliciet via `-IsccPath` of `ISCC_PATH` worden opgegeven.

## Resteert / bewust niet uitgevoerd

- Publieke ondertekening/publicatie: nog niet uitgevoerd. Componentnotices en exacte broncodevoorziening voor onder andere Qt/par2 moeten compleet worden gemaakt; dit is nog geen volledig licentievrijgegeven publiek pakket.
- [Juridische controle en signingadvies](docs/LEGAL-REVIEW.md): **geen openbaar expliciet verbod op ngPost/ngPost+ gevonden** in de doorzochte bronnen. Dat is geen garantie over niet-openbare zaken of concrete activiteiten rond de app. Gebruiker bevestigt schriftelijke RAR-bundeltoestemming; de voorwaarden zijn niet ingezien. Beoordeling van publieke presentatie/ondersteuning door een IE/IT-advocaat blijft aanbevolen.
- Geen posts naar een echte Usenet-provider uitgevoerd; uitsluitend eigen fixtures op localhost. Langdurig posten, trage/netwerkpaden, echte Windows-kleurdialogen, handmatige installerpagina's op meerdere Windows-talen en automatisch uitschakelen van de computer zijn niet volledig beproefd.
- UI-smoke/renderingen gebruiken Qt offscreen; native Windows-interactie blijft een aanvullende gebruikerscontrole.
- Bestaande gebruikerswijzigingen in `release.md`, `release-old.md` en de losse schermafbeelding zijn behouden. Persoonlijke serverconfiguraties zijn niet in de pakketten opgenomen.

Projectingang en mapfuncties: [README.md](README.md). Het oorspronkelijke [onderzoeksplan](docs/ISSUES-PLAN.md) is historisch; deze status is de actuele bron.
