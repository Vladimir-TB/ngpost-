# Actuele projectstatus

Datum: **19 september 2026**. Fase: **release 5.1.2 gepubliceerd**. Opleverstatus: **publieke nieuwste GitHub-release; installer en portable UNSIGNED; alleen installer en portable als toegevoegde downloads; hashes geverifieerd**.

## Gereed

- [GitHub-gebruikerswiki](https://github.com/Vladimir-TB/ngpost-/wiki) aangemaakt: Home, Werking van de app, Geschiedenis en Laatste verbeteringen, met zijbalknavigatie. Gepubliceerd via Vladimir-TB; wiki-commit `f5eb675`. Pagina-inhoud en interne links gecontroleerd.

- **Help → Nieuw in 5.1.2** bevat verbeteringen, opgeloste problemen en update-instructies in Nederlands en Engels. Makerscredits hersteld; ongefundeerde anonimiteitsclaim vervangen.
- Qt- en par2-licenties/attributions en RAR EULA worden meegeleverd. PAR2-binary is byte-identiek aan de officiële release; Qt-bronarchieven hebben de officiële hashes. Overeenkomstige broncode is direct gelinkt op de releasepagina; de gecombineerde bronpakketten zijn uit de GitHub-assets verwijderd. [Broncode en licenties](docs/RELEASE.md).
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

- [Publieke GitHub-release 5.1.2](https://github.com/Vladimir-TB/ngpost-/releases/tag/v5.1.2), gepubliceerd op 19 september 2026 om 15:40 CEST door **Vladimir-TB**. Release-ID `392085334`, geen draft/prerelease. De twee resterende binary-assets komen overeen met de oorspronkelijke lokale SHA-256. De twee bronpakketten en checksum-asset zijn op verzoek verwijderd; checksums staan nu in de releasetekst. Bouwinstructies zijn uit de huidige develop-documentatie verwijderd en blijven in de onveranderde release-tag bewaard. [Publicatiebewijs](artifacts/release-presentation/release-after.json).

- [Eindproducten en uitleg](release/5.1.2-UNSIGNED/00%20-%20START%20HIER.md).
- [Installer](release/5.1.2-UNSIGNED/ngPost-setup-v5.1.2-UNSIGNED.exe).
- [Portable ZIP](release/5.1.2-UNSIGNED/ngPost-portable-v5.1.2-UNSIGNED.zip).
- [Direct bijgewerkte portable app](dist-qt6/ngPost.exe). Een reeds draaiend proces moet opnieuw worden gestart om deze wijzigingen te zien.
- Appversie `5.1.2`; Windows-bestandsversie `5.1.2.0`. Productiebuild, lokale portable en geteste pakketpayload hebben dezelfde executable.
- SHA-256 executable: `3DF7B006E5D93165FC7AC3F81CF397EEDA4115A1855E017AB0F46C8F3638EED1`.
- [SHA-256 van installer en ZIP](release/5.1.2-UNSIGNED/SHA256SUMS.txt).
- Bronidentiteit: release-tag `v5.1.2`, commit `13526fb` (deze exacte bron hoort bij de binary en bron-ZIP). De opvolgende documentatiecommit legt alleen de geslaagde publicatie vast. Basis van de functionaliteit: `bc9b8df`. Publicatieaccount via GitHub API bevestigd als **Vladimir-TB**, repository **Vladimir-TB/ngpost-**.

## Bewezen/getest

| Controle | Resultaat / bewijs |
|---|---|
| Qt 6.8.3 / MSVC releasebuild | Geslaagd; [buildlog](artifacts/release-compliance/release-build.log). |
| 10 / 100 / 500 mappen | Alle sessies precies eenmaal, huidige tab behouden, annuleren binnen één seconde. 500 mappen: **25,05 s**, grootste GUI-pauze **82 ms**, p95 **68 ms**. [500-log](artifacts/issue-validation/result-500.log). |
| Nulmeting | Oude route: 100 mappen circa 11 s GUI-blokkade; 500 mappen na meer dan zeven minuten nog vast in GUI, gestopt. [Afgebroken nulmeting](artifacts/issue-validation/baseline-500-aborted.txt). |
| Wachtrij en jobafronding | 5 en 100 sessies tegen lokale NNTP-testserver; volgorde, NZB-segmenten, dubbelklikken, vier conflictkeuzes, dubbele doelen, ontbrekende bron, stoppen/herstarten. PREPARE_PACKING zowel uit als aan. Grootste gemeten GUI-pauze bij deze wachtrijen 46 ms. [Uit](artifacts/session-workflow/result-prepare-false.log), [aan](artifacts/session-workflow/result-prepare-true.log). |
| Kopiëren en externe processen | Unicode, spaties, voorloopnullen, vast/sessiewachtwoord, metadata na afronding, echte RAR-compressie, onstartbaar hulpmiddel en annuleren van een vastlopend hulpmiddel. |
| Help en thema | Nederlands/Engels releaseoverzicht aanwezig en gerenderd; acht kleuren in licht/donker; tekstcontrast minimaal 4,5:1; kiezen, annuleren, opslaan, herstart en Reset. [Smoke](artifacts/colorpicker/smoke.log), [herstart](artifacts/colorpicker/restart.log). |
| Installer en portable | Inno 7.1.0; verse Engelse setup, expliciet Nederlands, bewaarde taal, normale/portable modus, doelmap, configuratiebehoud, identieke executable en portable starten zonder Qt op PATH. [Bewijs](artifacts/installer-smoke/20260919-153622/result.txt). |

De controles gebruiken geïsoleerde fixtures onder `artifacts/`; bestaande gebruikersinstallaties en persoonlijke configuraties worden niet gebruikt.

## Externe wijzigingen en herstel

- Scope: GitHub-release v5.1.2, develop-documentatie en de wiki van Vladimir-TB/ngpost-. De twee binaries en release-tag zijn ongewijzigd. Drie aanvullende release-assets verwijderd op verzoek; bronlinks en inline checksums staan in de releasetekst. GitHub houdt zelf twee automatische broncodearchieven zichtbaar.
- Herstel: oorspronkelijke releasemetadata in `artifacts/release-presentation/release-before.json`; oorspronkelijke downloads lokaal in `release/5.1.2-UNSIGNED`. Bronbackups voortaan in `artifacts/source-backups/5.1.2`; het backupscript schrijft geen nieuwe bronassets naar de release-output. Wiki heeft eigen Git-historie en een lokale checkout in `artifacts/wiki-publishing`.

## Resteert / bewust niet uitgevoerd

- Ondertekening is niet uitgevoerd; installer en app blijven herkenbaar UNSIGNED. Licentieteksten, componentbronnen en provenance zijn technisch aangevuld; dit is geen advocatenverklaring of volledige juridische/security-audit.
- [Juridische controle en signingadvies](docs/LEGAL-REVIEW.md): **geen openbaar expliciet verbod op ngPost/ngPost+ gevonden** in de doorzochte bronnen. Dat is geen garantie over niet-openbare zaken of concrete activiteiten rond de app. Gebruiker bevestigt schriftelijke RAR-bundeltoestemming; de voorwaarden zijn niet ingezien. Beoordeling van publieke presentatie/ondersteuning door een IE/IT-advocaat blijft aanbevolen.
- Geen posts naar een echte Usenet-provider uitgevoerd; uitsluitend eigen fixtures op localhost. Langdurig posten, trage/netwerkpaden, echte Windows-kleurdialogen, handmatige installerpagina's op meerdere Windows-talen en automatisch uitschakelen van de computer zijn niet volledig beproefd.
- UI-smoke/renderingen gebruiken Qt offscreen; native Windows-interactie blijft een aanvullende gebruikerscontrole.
- Bestaande gebruikerswijzigingen in `release.md`, `release-old.md` en de losse schermafbeelding zijn behouden. Persoonlijke serverconfiguraties zijn niet in de pakketten opgenomen.

Projectingang en mapfuncties: [README.md](README.md). Het oorspronkelijke [onderzoeksplan](docs/ISSUES-PLAN.md) is historisch; deze status is de actuele bron.
