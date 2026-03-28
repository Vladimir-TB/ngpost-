# NgPost + v5.1.1 Qt 6 release

Deze release bevat alleen de nieuwe wijzigingen sinds `v5.1.0`.
Alle vernieuwingen uit `v5.1.0` blijven gewoon behouden.

## Nieuw in v5.1.1

- `Geavanceerd tonen` blijft nu per scherm bewaard. Als een gebruiker dit opent in `Snel posten` of `Automatisch posten`, blijft dat na opslaan en herstart zo staan tot de gebruiker het zelf weer sluit.
- De optie `archief wachtwoord voor deze post` in `Snel posten` heeft nu een eigen bewaarde aan/uit-keuze. Staat die uit, dan blijft die ook uit na `Config opslaan` en na een herstart.
- Het vaste archiefwachtwoord in `Voorkeuren` staat standaard uit. Pas als een gebruiker het zelf invult en aanvinkt, blijft het actief tot het weer wordt uitgezet.
- De terminologie rond wachtwoorden is duidelijker gemaakt. De app onderscheidt nu beter tussen een vast standaard wachtwoord en een wachtwoord voor een specifieke post.
- Een bug met Usenet-provider gebruikersnaam en wachtwoord is opgelost: waarden die met een `0` beginnen blijven nu correct bewaard en geladen.
- Voorkeuren die gebruikers aan- of uitvinken worden nu betrouwbaarder opgeslagen in de config, ook bij meerdere gerelateerde opties.
- De schaalfunctie is verder verbeterd. Vooral op `123%` en `150%` schalen knoppen en layout nu consistenter mee.
- De knop `Geavanceerd tonen/verbergen` blijft nu ook op hogere schaalniveaus goed leesbaar.
- De `Snel posten` en `Automatisch posten` werkruimte is verder opgeschoond, met een rustiger geavanceerd blok onder de hoofdactie.
- `par2.exe` wordt nu meegeleverd in de Qt 6 distributie, en de koppeling tussen `maak par2` en de `%`-instelling in de GUI werkt beter.
- De Qt 6 buildscript doet nu een schone rebuild voor versie- en resourcewijzigingen, zodat nieuwe versienummers en icoonupdates altijd correct in de exe terechtkomen.

## Vernieuwingen t.o.v. originele ngPost (mbruel/ngPost) in v5.1.0 baseline

Deze punten zijn toegevoegd of aangepast in ngPost+ t.o.v. upstream (v4.16) en vormen de basis van `v5.1.0`:

Toevoegingen:
- Donker/licht schakelaar in de GUI.
- Thema‑kleurkiezer met reset‑knop.
- Thema‑kleur wordt opgeslagen in `ngPost.conf` (`THEME_COLOR`) en herstelt bij start.
- Inno Setup installer met Nederlandse voorwaarden/waarschuwing.
- Automatische installatie van `vc_redist.x64.exe` als die ontbreekt.
- Startmenu‑snelkoppeling + optionele desktop‑icoon.
- SHA256 checksum bestand voor de installer.

Wijzigingen:
- Upgrade toolchain naar Qt 5.15.2.
- GUI draait zonder zichtbare console (Windows subsystem).
- About‑tekst opgeschoond en aangepast.
- Taalondersteuning beperkt tot EN/DE/NL (NL standaard).

Verwijderd:
- Donatieknop en Bitcoin‑knoppen inclusief achterliggende code en assets.

Opmerking:
- Upstream heeft geen `v5.1.0`; deze baseline hoort bij ngPost+.
- Upstream release notes staan volledig in `upstream_release_notes.txt` (t/m v4.16).

## Installer

- Installerbestand:
  `ngPost-setup-v5.1.1.exe`
- SHA256:
  `473D94DA44F6BE666A0DE4C143E3E440A5B62117B99E2267946FBEE2D31FCA88`

## Opmerking

Deze `v5.1.1` release is een verfijningsupdate op de Qt 6 editie.
De focus ligt op stabielere configuratie-opslag, duidelijker wachtwoordgedrag, betere schaalbaarheid en een nettere dagelijkse workflow.
