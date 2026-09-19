# Uitvoerbaarheidsplan GitHub-issues 1, 2 en 3

Onderzoeksdatum: 19 september 2026. Onderzochte lokale bron: `3e82949` (Qt 6.8.3, bronversie 5.1.1). Doel: aanpak en acceptatiecriteria bepalen; de hieronder beschreven functies zijn nog niet geïmplementeerd.

## Advies

Alle concrete verzoeken zijn uitvoerbaar binnen de bestaande native Qt-app. Geef betrouwbaarheid bij grote aantallen mappen voorrang. Gebruik vervolgens dezelfde voorbereiding en bestaande wachtrij voor de nieuwe knop **Alle sessies starten**. Kopieerknoppen en de installervertaling zijn kleinere, afgebakende wijzigingen.

De openbare issues én alle reacties zijn gelezen via GitHub. Issue 3 bevat twee verzoeken. Issue 1 is gesloten, maar de lokale installerbron bevat nog uitsluitend Nederlands. Bij controle wees remote HEAD naar `11af4b5`; de lokale themawijzigingen zijn dus nog geen bewijs van een gepubliceerde update.

| Verzoek | Haalbaarheid | Omvang / risico |
|---|---|---|
| [#2: vastlopen bij 400–500 mappen](https://github.com/Vladimir-TB/ngpost-/issues/2) | Goed uitvoerbaar; definitieve oorzaak eerst meten | Grootste wijziging; lifecycle van jobs, threads en UI beschermen |
| [#3: kopiëren van naam en wachtwoord](https://github.com/Vladimir-TB/ngpost-/issues/3) | Direct mogelijk met bestaande Qt-functionaliteit | Klein; juiste sessiewaarden gebruiken |
| [#3, aanvullende reactie: sessies achter elkaar starten](https://github.com/Vladimir-TB/ngpost-/issues/3#issuecomment-5002581774) | Bestaande wachtrij biedt de basis | Middelgroot; geen dubbele jobs, overschrijven en annuleren zorgvuldig afhandelen |
| [#1: installer standaard Engels](https://github.com/Vladimir-TB/ngpost-/issues/1) | Goed uitvoerbaar met Inno Setup | Klein; ook eigen wizardteksten vertalen |

## 1. Vastlopen bij veel mappen

**Vraag:** de app blijft lang hangen bij honderden mappen, na bevestigen van bestaande NZB-bestanden en tussen opeenvolgende posts. De reactie met het overschrijfscenario is een belangrijk onderdeel van de reproductie, niet een apart cosmetisch probleem.

**Vastgesteld in de code:**

- `AutoPostWidget::onGenQuickPosts()` in `src/hmi/AutoPostWidget.cpp:251` maakt voor iedere map direct een compleet sessievenster en optioneel een postingjob aan, in één GUI-callback.
- `MainWindow::addNewQuickTab()` in `src/hmi/MainWindow.cpp:2243` initialiseert het venster al. De aanroeper doet daarna opnieuw `init()`. Die methode legt onder andere signaalverbindingen aan (`src/hmi/PostingWidget.cpp:457`). Dubbele initialisatie en verbindingen zijn concreet aanwezig.
- Iedere nieuwe tab selecteert zichzelf en vernieuwt de tabweergave. `_refreshWorkspaceTabBar()` loopt alle tabs langs (`src/hmi/MainWindow.cpp:1864`). De totale hoeveelheid UI-werk groeit daardoor ongunstig met het aantal sessies.
- `PostingWidget::postFiles()` toont bij een bestaand NZB een modale vraag voordat de job wordt ingepland (`src/hmi/PostingWidget.cpp:173`). Dit staat midden in de batchroute.
- Bij afronden roept `PostingJob::_finishPosting()` de methode `Poster::stopThreads()` aan; daarin staan blokkerende `wait()`-aanroepen (`src/Poster.cpp:169`). Bij stoppen van een extern proces staat ook `waitForFinished()` (`src/PostingJob.cpp:447`).
- Recursief bepalen van mapgroottes kan eveneens blokkeren, maar de gevonden route is afhankelijk van `TMP_RAM`. Dat is niet zonder meer de algemene oorzaak.

**Nog niet bewezen:** welke stap de gemelde urenlange vertraging domineert. Er is in dit onderzoek geen batch van de melder gereproduceerd, geen performanceprofiel gemaakt en geen live post uitgevoerd. De bovenstaande punten zijn codebevindingen en onderzoekshypothesen, geen gemeten snelheidswinst.

**Aanpak:**

1. Maak reproduceerbare fixtures met 10, 100 en 500 mappen, uiteenlopende bestandsaantallen en drie vooraf bestaande NZB's. Meet scannen, tabs aanmaken, validatie, inplannen en overgang tussen jobs afzonderlijk. Meet ook geheugen en de langste onderbreking van de GUI-eventloop.
2. Scheid bestanden verzamelen en valideren van widgets aanmaken. Laat schijfwerk in een worker uitvoeren; widgets blijven op de GUI-thread. Voeg sessies in kleine, begrensde porties toe met voortgang en annuleren. Gebruik de [Qt 6.8 worker-objectaanpak](https://doc.qt.io/qt-6.8/qthread.html).
3. Verwijder de dubbele initialisatie. Bundel tablabels/layoutupdates en behoud de huidige actieve tab tijdens batchtoevoeging. Voeg alleen een grotere wijziging zoals uitgesteld aanmaken van sessievensters toe als metingen laten zien dat dit noodzakelijk is.
4. Verzamel naamconflicten vóór starten. Bied **overslaan**, **overschrijven**, **unieke naam** en **batch annuleren**, met een keuze voor alle conflicten. Controleer ook dubbele doelpaden binnen dezelfde batch en bestanden die na de voorcontrole ontstaan. Overschrijven vereist een expliciete keuze.
5. Laat afronding van threads en externe processen via signalen verlopen. Houd time-outs, annulering en objectlevensduur expliciet bij. Het uploadprotocol en artikelverwerking alleen wijzigen wanneer meetbewijs dat noodzakelijk maakt.

**Acceptatie:** alle bedoelde mappen verschijnen precies eenmaal; de drie conflicten werken in iedere keuzeroute; annuleren laat een consistente wachtrij achter; geen onbedoeld overschrijven. Voorstel voor de lokale SSD-fixture: 95% van GUI-timerintervallen binnen 100 ms, geen blokkade langer dan 500 ms en bevestiging van annuleren binnen één seconde. Dit zijn te behalen criteria, geen huidige resultaten. Voor netwerkpaden blijft de UI responsief, ook wanneer schijf-I/O langer duurt. Herhaal overgangen tussen jobs met een lokale NNTP-testserver, inclusief fout, stop en herstart van de app.

## 2. Alle voorbereide sessies achter elkaar starten

**Bestaande basis:** `NgPost::startPostingJob()` (`src/NgPost.cpp:2567`) gebruikt één `_activeJob` en `_pendingJobs`. `onPostingJobFinished()` start daarna de volgende job. Automatisch posten kan al nieuw gegenereerde sessies meteen inplannen; de ontbrekende bediening betreft alle reeds voorbereide handmatige sessies.

**Aanpak:** voeg **Alle sessies starten** toe bij Snel posten. Neem een momentopname van gereedstaande, nog niet gestarte sessies in tabvolgorde. Valideer eerst, behandel conflicten via de batchroute uit onderdeel 1 en voeg daarna aan de bestaande wachtrij toe. Een reeds actieve job loopt door; nieuwe jobs komen achter de bestaande wachtrij. Sluit actieve, reeds ingeplande, lege en voltooide sessies uit. Een tweede klik mag niets dubbel inplannen of stoppen.

Gebruik hiervoor een expliciete start-/enqueuefunctie. Blind alle `postFiles()`-callbacks aanroepen is ongeschikt: diezelfde functie stopt een sessie als deze al aan het posten is. Sla instellingen per sessie vast zodat de naam, het wachtwoord en compressieopties van de ene tab niet door een andere worden overschreven.

**Sequentieel betekent:** maximaal één uploadende postingjob, met behoud van meerdere NNTP-verbindingen binnen die job. De bestaande optie `PREPARE_PACKING` kan ondertussen het volgende archief voorbereiden. Aanbeveling: die bestaande gebruikersinstelling respecteren en dit in de toelichting benoemen; volledig sequentieel inpakken én posten werkt met die optie uit.

**Acceptatie:** vijf en honderd sessies volgen de verwachte volgorde; maximaal één actieve uploadjob; dubbelklikken dupliceert niets; annuleren/starten tijdens een actieve job blijft correct. Test verschillende wachtwoorden per sessie, ongeldige sessies, verwijderde bronbestanden, sluiten van een tab, fouten en afsluiten na voltooiing. De batchregistratie moet voorkomen dat automatisch afsluiten afgaat terwijl nog sessies worden toegevoegd. Een wachtrij bewaren over een app-herstart is geen onderdeel van dit verzoek.

## 3. Naam en wachtwoord kopiëren

`PostingWidget` bevat `compressNameEdit`, `nzbFileEdit` en `nzbPassEdit`. Qt biedt [QClipboard::setText()](https://doc.qt.io/qt-6.8/qclipboard.html); er is geen nieuwe afhankelijkheid nodig.

**Voorstel:** een kleine kopieerknop naast Archiefnaam en Wachtwoord. Omdat de issue niet aangeeft of “Filename” de archiefnaam of NZB-bestandsnaam bedoelt, neem ook een afzonderlijke, duidelijk gelabelde kopieeractie bij het bestaande NZB-veld op. Kopieer daar de bestandsnaam met extensie, zonder het lokale mappad. Geen nieuwe exportfunctie of verplicht tekstbestand.

De wachtwoordactie moet hetzelfde effectieve archiefwachtwoord gebruiken als het posten: sessiewachtwoord wanneer ingesteld, anders het ingeschakelde vaste archiefwachtwoord. Dit is nu bepaald in `PostingWidget::udatePostingParams()` (`src/hmi/PostingWidget.cpp:653`). Breng die bepaling onder in één gedeelde functie. Voor een ingeplande/actieve job is de vastgelegde jobwaarde leidend. Als kopiëren na voltooiing mogelijk blijft, bewaar dan de definitieve metadata bij de sessie: de `PostingJob` wordt na afloop verwijderd.

**Acceptatie:** exacte tekst bij plakken, inclusief voorloopnullen, spaties, Unicode en speciale tekens; juist wachtwoord bij vaste en per-sessie instellingen; geen wachtwoord kopiëren wanneer compressie/wachtwoord niet van toepassing is. Geen wachtwoord in log, tooltip of bevestiging; uitsluitend een korte melding “Gekopieerd”. Test ook na naamgeneratie, inplannen en afronden, plus thema's en UI-schaal 100–150%.

## 4. Installer standaard Engels

`installer/ngPost.iss:44` bevat alleen `dutch`. Ook de installatiemodus, portable-mapkeuze en runtime-installatiemelding zijn hardcoded Nederlands. Alleen de Engelse taaldefinitie toevoegen is daarom onvoldoende.

**Aanpak:** Engels als eerste taal toevoegen, Nederlands behouden en `LanguageDetectionMethod=none` gebruiken voor Engels als standaard bij een verse installatie. Laat `UsePreviousLanguage=yes` bestaande keuzes bij upgrades respecteren. Dit onderscheid volgt de [Inno Setup-taaldetectie](https://jrsoftware.org/ishelp/topic_setup_languagedetectionmethod.htm) en [hergebruik van de vorige taal](https://jrsoftware.org/ishelp/topic_setup_usepreviouslanguage.htm).

Verplaats eigen wizardteksten naar [CustomMessages](https://jrsoftware.org/ishelp/topic_custommessagessection.htm) met Engelse en Nederlandse vertalingen. Maak de bestaande korte voorwaarden taalafhankelijk met behoud van de inhoud. Respecteer een expliciete taalkeuze en `/LANG=english` of `/LANG=dutch`.

De app zelf start momenteel standaard Nederlands (`src/NgPost.cpp:296`). Dat is een afzonderlijke keuze: dit issue vraagt om de installertaal. De bestaande appvoorkeur wordt niet gewijzigd; Engels als appstandaard alleen als apart vervolg besluiten.

**Acceptatie:** verse installatie op Nederlandse en Engelse Windows begint met Engels als standaardkeuze; Nederlands is selecteerbaar; alle wizardpagina's en voorwaarden volgen de keuze. Test normale installatie, portable, upgrade met bewaarde taal, silent installatie en beide `/LANG`-opties. Verifieer behoud van configuratie en de portable-marker.

## Aanbevolen uitvoervolgorde en oplevering

1. Maak voor #2 een meetbare reproductie en leg een nulmeting vast.
2. Herstel batchvoorbereiding, conflictafhandeling en blokkerende jobovergangen; laat de 500-mappentest slagen.
3. Voeg de kopieeracties toe en daarna Alle sessies starten op de geteste batch-/wachtrijroute.
4. Vertaal de installer en controleer schone installatie en upgrade.
5. Voer de gezamenlijke regressiecontrole uit: thema, schaal, configuratie, compressie/PAR2, queue en NNTP-fouten. Maak daarna één releasekandidaat met afgestemde versies, hashes en installatietest.

Ruwe inschatting, inclusief gerichte tests: #2 circa 3–6 ontwikkeldagen; kopieeracties 0,5–1 dag; alle sessies starten 1–2 dagen; installer 0,5–1 dag; gezamenlijke releasecontrole 1–2 dagen. Totaal circa **6–12 ontwikkeldagen**. Herijk na de nulmeting; dit is geen kalenderbelofte.

Houd productcode onder `src/`, herhaalbare tests onder `tests/`, metingen en fixtures onder `artifacts/issue-validation/` en eindproducten in de bestaande distributiestructuur. Werk de build-/packageregels mee bij als staging verandert. Bewaar per wijziging een afgebakende commit en werk [STATUS.md](../STATUS.md) bij. Issues beantwoorden of sluiten en een release publiceren zijn afzonderlijke acties na uitvoering; tijdens dit onderzoek is niets op GitHub geplaatst.
