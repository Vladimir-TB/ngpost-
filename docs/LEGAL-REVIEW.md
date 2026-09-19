# Juridische controle en advies over ondertekening

Onderzocht op 19 september 2026, vanuit Nederlands/EU-perspectief. Dit is een technische licentie-inventarisatie met juridische risicoanalyse, geen individueel advocatenadvies. De actuele opleverstatus staat uitsluitend in [STATUS.md](../STATUS.md).

## Advies

**Expliciete verbodscontrole:** op 19 september 2026 is in de doorzochte openbare bronnen geen wet, gepubliceerde uitspraak of gepubliceerde BREIN-maatregel gevonden die ngPost of deze ngPost+-fork bij naam verbiedt. Gezocht op `ngPost`, `ngPost+`, `Vladimir-TB/ngpost-`, gecombineerd met verbod, BREIN, rechtszaak, court, injunction en banned, met gerichte zoekopdrachten voor Rechtspraak, BREIN en Europese rechtspraak. De zoekmachines gaven geen relevant expliciet verbod. De interne zoekpagina's van BREIN/Rechtspraak waren via de gebruikte browserdienst niet uitleesbaar; dit was geen uitputtende registercontrole. Niet-openbare sommaties, niet-gepubliceerde procedures en toekomstige maatregelen zijn hiermee niet uitgesloten. Een ontbrekend verbod is geen afzonderlijke juridische goedkeuring van alle activiteiten rond de app.

Een algemene Usenet-postingclient is niet uitsluitend door zijn uploadfunctie verboden. Rechtmatige toepassingen bestaan, zoals verspreiding van eigen werk met alle benodigde rechten. Er is echter geen algemene vrijwaring voor de maker: de daadwerkelijke inrichting, presentatie, ondersteuning en betrokkenheid bij auteursrechtinbreuk zijn relevant. Uit deze broncontrole volgt geen garantie dat ngPost+ juridisch zonder risico kan worden uitgegeven.

Laat vóór openbare distributie onder de naam van de certificaathouder een Nederlandse IE/IT-advocaat de app, downloadpagina, handleidingen, ondersteuning en banden met eventuele uploadcommunities beoordelen. Code signing kan daarna zinvol zijn voor herkenbare herkomst en integriteit. Ondertekening is geen juridische of veiligheidsgoedkeuring; ook zonder handtekening bestaan verplichtingen als aanbieder.

## BREIN en postingfunctionaliteit

- In FTD/BREIN oordeelde de rechtbank dat het structureel faciliteren en stimuleren van illegaal uploaden in die specifieke platformcontext onrechtmatig was. Dat is geen uitspraak die alle uploadsoftware verbiedt. Gebruik oudere passages over downloaden uit illegale bron niet als huidige vrijbrief. Bron: [Rechtbank Haarlem, 9 februari 2011, met name 4.21–4.25](https://uitspraken.rechtspraak.nl/details?id=ECLI:NL:RBHAA:2011:BP3757).
- Het Europese Filmspeler-arrest illustreert dat bewuste inrichting en reclame voor toegang tot ongeautoriseerde werken zwaar kunnen wegen. Dit betrof andere feiten dan een losse uploadclient; toepassing op ngPost+ vereist een afzonderlijke beoordeling. Bron: [HvJ EU, zaak C-527/15](https://curia.europa.eu/jcms/upload/docs/application/pdf/2017-04/cp170040en.pdf).
- BREIN meldt zelf actief op te treden tegen Usenet-uploaders van ongeautoriseerd materiaal. Dit is informatie van een belangenbehartiger, geen onafhankelijke uitspraak dat iedere postingclient verboden is. Bron: [BREIN over Usenet-uploaders](https://stichtingbrein.nl/grote-usenet-uploader-schikt-met-brein/).

In de onderzochte code kiest de gebruiker lokale bestanden en configureert deze eigen NNTP-servers. Er zijn compressie, PAR2, NZB-export, monitoring en obfuscatie. Deze eigenschappen zijn op zichzelf geen bewijs van onrechtmatig handelen, maar beloftes over onvindbaar zijn of hulp bij concrete illegale uploads kunnen de context veranderen. De beschrijving in `src/NgPost.h` noemt momenteel een 'unique invisible mode' en 'safe' posting: onderbouw of verduidelijk dit; beloof geen anonimiteit of bescherming tegen handhaving. Dit onderzoek omvat geen volledige controle van externe websites, besloten supportgesprekken of daadwerkelijke gebruikersactiviteiten.

De oorspronkelijke twee regels in `installer/voorwaarden.txt` over misbruik en ongeautoriseerd materiaal waren geen volledige licentie en verschaften op zichzelf geen aansprakelijkheidsvrijstelling. Op verzoek is nu een zichtbare appmelding en genuanceerde Nederlandse/Engelse gebruikstekst toegevoegd: de gebruiker moet verspreidingsrechten hebben; wettelijke aansprakelijkheid en rechten onder componentlicenties worden niet uitgesloten. Een waarschuwing kan tegenstrijdige feitelijke activiteiten niet herstellen.

## Gecontroleerde onderdelen

| Onderdeel | Lokale bevinding | Benodigd voor openbare uitgifte |
|---|---|---|
| ngPost / ngPost+ | GPLv3 in `LICENSE`, nu ook opgenomen in beide kandidaten; oorspronkelijke copyrightregels in de bron. README noemt de oorspronkelijke maker. De huidige About-regel vermeldt alleen spotnet.team. | Passende oorspronkelijke én nieuwe credits, wijzigingsdatum en exact bij de binary horende complete broncode met buildscripts beschikbaar maken. |
| Qt 6.8.3 | Dynamische Qt DLL's en plugins meegeleverd; er is geen licentiemap in de huidige `dist-qt6`. | Licentie-/copyrightteksten voor de gebruikte modules en ingesloten derden, passende broncodevoorziening en behoud van toepasselijke vervangings-/wijzigingsrechten. Alleen dynamisch linken is niet alle verplichtingen nakomen. |
| RAR 7.20 | Losse `rar.exe`, geldig ondertekend door de oorspronkelijke leverancier. | Gebruiker bevestigde op 19 september 2026 schriftelijke bundeltoestemming. Toestemming is niet ingezien: controleer of deze versie, installer, portable, distributiekanalen, licentieteksten en rechten van eindgebruikers dekt. Een gewone gebruikslicentie verleent niet automatisch bundelrechten. |
| par2cmdline-turbo 1.3.0 | Geïdentificeerd met `par2.exe -V`; geen copyright-/bronbestand bij de binary. | De bij deze binary horende herkomst/build en GPLv2-voorwaarden vastleggen; broncodevoorziening en vereiste notices inclusief ingebouwde afhankelijkheden leveren. |
| Microsoft runtime / DirectX DLL's | `vc_redist.x64.exe`, `dxcompiler.dll`, `dxil.dll` aanwezig. | Redistributierechten en toepasselijke notices controleren tegen de gebruikte officiële distributie. Dit is nog geen volledige SBOM/licentie-audit van alle plugins. |
| Naam, logo en uitgever | Naam ngPost+, branding spotnet.team; bestaande broncodecredits aanwezig. | Recht op gebruikte assets/merken en de werkelijke uitgeversidentiteit bevestigen. Een merknaam is niet automatisch de juridische certificaathouder. |

De GPL staat verspreiding van wijzigingen toe onder voorwaarden, waaronder licentienotices en passende Corresponding Source. Zij maakt meegeleverde zelfstandige proprietary tools niet automatisch GPL; iedere afzonderlijke component moet rechtmatig verspreid worden. Bronnen: [GPLv3, onderdelen 4–6](https://www.gnu.org/licenses/gpl.en.html), [Qt GPL/LGPL-verplichtingen](https://www.qt.io/development/open-source-lgpl-obligations), [RAR EULA, onderdelen 2–3](https://www.rarlab.com/license.htm), [par2cmdline-turbo v1.3.0 COPYING](https://github.com/animetosho/par2cmdline-turbo/blob/v1.3.0/COPYING).

## Wat ondertekenen betekent

Authenticode koppelt een bestand aan de geverifieerde uitgever en maakt wijzigingen na signing detecteerbaar. De naam van de certificaathouder is zichtbaar. Het verleent geen rechten op content of bibliotheken, voorkomt geen aansprakelijkheid en bewijst niet dat software foutloos is. Bron: [Microsoft Authenticode](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/authenticode).

Een geldige handtekening kan uitgeversreputatie opbouwen; nieuwe bestanden kunnen nog steeds SmartScreen-waarschuwingen geven. Ook EV is geen automatische bypass. Potentieel ongewenst gedrag kan de reputatie schaden. Gebruik een juiste, consistente identiteit en bescherm de sleutel. Bron: [Microsoft SmartScreen voor ontwikkelaars](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation).

Bij een latere goedgekeurde release: eerst de definitieve eigen `ngPost.exe` tekenen en verifiëren, vervolgens deze identieke executable in installer en portable opnemen, daarna de installer tekenen en timestamp/signature/hash controleren. Laat bestaande leveranciershandtekeningen intact. Een ZIP is op zichzelf geen Authenticode-signed executable; de app erin kan wel ondertekend zijn. Er is in deze opdracht geen certificaat gebruikt en geen publieke distributie verricht.
