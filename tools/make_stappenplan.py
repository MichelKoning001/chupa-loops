#!/usr/bin/env python3
"""Builds "Stappenplan Chupa Loops.pdf" - the Dutch step-by-step guide that goes
with the delivery (project -> GitHub -> installer), next to the English manual.

    python3 tools/make_stappenplan.py [output folder]

Shares its look and its helpers with tools/make_manual.py.
"""

import os
import sys

from reportlab.lib.units import mm
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_LEFT
from reportlab.platypus import Paragraph, Spacer, PageBreak, Image as RLImage

import make_manual as M


def build_story():
    s = []
    s.append(Paragraph('<font color="#2B1533">Chupa</font> <font color="#FF2E88">Loops</font>',
                       ParagraphStyle("title", fontName="Erica", fontSize=26, leading=30,
                                      alignment=TA_LEFT)))
    s.append(M.P("Stappenplan fase 1: van project naar installer. De Engelse handleiding voor je "
                 "gebruikers zit apart in de zip en in het project (docs).", M.small))
    s.append(Spacer(1, 3 * mm))
    s += M.picture(M.shot("screenshot.png"), M.CONTENT_W)

    s.append(M.H("Deel A: op GitHub zetten (eenmalig, ca. 20 min)"))
    s.append(M.steps([
        "Log in op <b>github.com</b>. Maak een nieuwe repository: <b>+ &gt; New repository</b>, "
        "naam <b>chupa-loops</b>, <b>Public</b>, <b>Create repository</b>. (Je oude "
        "slice-tribe-repository mag je laten staan of verwijderen, die gebruiken we niet meer.)",
        "Klik op <b>uploading an existing file</b>. Open in Finder de map <b>1 - Project voor "
        "GitHub/ChupaLoops</b>, druk <b>Cmd + Shift + .</b> (punt) zodat je de verborgen map "
        "<b>.github</b> ziet, selecteer alles (<b>Cmd + A</b>) en sleep het in het uploadvak. "
        "Sleep de inhoud van de map, niet de map zelf. (Het zijn ongeveer 50 bestanden; GitHub "
        "accepteert er maximaal 100 per keer. Zet er dus geen mappen als <b>build</b> of "
        "<b>third_party</b> bij.)",
        "Klik <b>Commit changes</b>. Controleer dat de map <b>.github</b> bovenaan in je "
        "repository staat.",
        "Ga naar <b>Actions</b>. De build start vanzelf en duurt 20 tot 30 minuten. Een groen "
        "vinkje = klaar.",
        "Op de hoofdpagina staat rechts onder <b>Releases</b>: Chupa Loops (latest build "
        "#nummer). Daar download je <b>ChupaLoops-macOS.dmg</b> en "
        "<b>ChupaLoops-Windows-Setup.exe</b>. Elke keer dat je iets nieuws uploadt, wordt die "
        "release automatisch bijgewerkt.",
    ]))

    s.append(M.H("Deel B: installeren"))
    s.append(M.steps([
        "<b>Mac:</b> open ChupaLoops-macOS.dmg en dubbelklik <b>Chupa Loops Installer.pkg</b>. "
        "Zolang de installer nog niet door Apple is ondertekend (Deel D), zegt je Mac dat hij hem "
        "niet kan controleren: ga dan naar <b>Systeeminstellingen &gt; Privacy en beveiliging</b> "
        "en klik <b>Open toch</b>.",
        "<b>Windows:</b> start ChupaLoops-Windows-Setup.exe. Bij een SmartScreen-melding: "
        "<b>Meer informatie &gt; Toch uitvoeren</b>.",
        "Start Fender Studio Pro (of Logic, Cubase, Ableton, FL Studio) opnieuw. Chupa Loops staat "
        "bij de <b>instrumenten</b>: Percep-tion &gt; Chupa Loops.",
    ]))

    s.append(PageBreak())
    s.append(M.H("Deel C: wat zit erin"))
    s.append(M.defs([
        ("Naam &amp; look", "Chupa Loops met 7 skins: Lolly (standaard, knalroze), Fruity "
                            "(snoepgroen), Skull, Butcher (met bloeddruppels), Neon, Acid en "
                            "Smile. Knop SKIN rechtsboven. Elke skin heeft een eigen lolly als "
                            "mascotte (schedel-lolly, lolly met hakmes, acid-lolly met "
                            "scheikundeflesje, smiley-lolly ...) die reageert op NEW LOOP."),
        ("Gekke knop", "In de knoppenkolom: de vaagste loop ever, per skin anders: SUGAR RUSH, "
                       "FRUIT PUNCH, SKULL DAMAGE, THE BUTCHER CUT, NEON OVERDRIVE, ACID FLASHBACK "
                       "en SMILEY MAYHEM. Ook via MIDI-noot E1. Te gek? Laad je preset opnieuw of "
                       "stap terug met ◀."),
        ("100 presets", "10 smaken met snoepnamen (Trance Treats, Hard Candy, Psy Sweets, Techno "
                        "Toffee ...). Klik op de presetnaam voor de browser met zoekveld; SAVE "
                        "bewaart je eigen presets."),
        ("MUTATE", "Knop onder NEW LOOP: een variatie op je loop, ongeveer een kwart van de "
                   "stukjes verandert, de rest blijft staan (MIDI F1). Nog een keer drukken geeft "
                   "weer een andere variatie."),
        ("NEW LOOP: knoppen", "Het schakelaartje onder MUTATE bepaalt wat NEW LOOP met het "
                              "CHARACTER | FX-paneel doet. <b>KEEP KNOBS</b> laat je instellingen "
                              "staan, <b>ALL NEUTRAL</b> zet dat paneel eerst weer neutraal. Je "
                              "ritme, lengte, slicing, FILL en volume blijven altijd staan."),
        ("Scenes A-H", "Bovenaan bij de samples: klik een lege scene om je loop te bewaren, klik "
                       "een bewaarde scene om hem terug te halen. Live wisselen met MIDI C4 t/m "
                       "G4."),
        ("Fill", "In het Rhythm-paneel: elke 4, 8, 16 of 32 bars wordt de laatste halve maat een "
                 "roll, zoals een drumfill."),
        ("FX", "Tab FX naast Character: filter met envelope per stukje, resonantie, low cut, "
               "drive, pump (sidechain) en stereobreedte. Alleen als afwerking over de hele loop."),
        ("Per sample", "Elk slot heeft een afspeelknopje (los beluisteren, op eigen tempo, met "
                       "meelopende streep), een aandeel in procenten (0% = nooit, 200% = twee keer "
                       "zo vaak), een <b>dB-veld</b> voor het niveau van dat sample (–24 tot "
                       "+24 dB) en in het rechtsklikmenu een eigen <b>STRETCH</b> (Beats of "
                       "Smooth) als dat ene sample iets anders nodig heeft dan de rest."),
        ("STR", "Rechtdrukken: trekt een menselijke opname op de maat - een oude plaat, een live "
                "take, een zang die zwabbert. Klikken zet het aan, slepen bepaalt hoe ver (100% = "
                "kaarsrecht, 60% haalt de wiebel eruit en houdt het gevoel)."),
        ("MY TRACK", "Links onder het resultaat zit een apart vak: DROP YOUR OWN TRACK HERE. Sleep "
                     "daar een stuk van je eigen track in (een maat of twee is genoeg). Dat wordt "
                     "niet gehakt: de nieuwe loop laat juist ruimte waar jouw track druk is, en de "
                     "toonsoort volgt hem. Het procentveld in het vak bepaalt hoe streng: 0% = "
                     "helemaal niet, 100% = normaal, 200% = echt uit de weg blijven. Elke maat "
                     "houdt altijd een paar stukjes over - en juist die op de rustigste plekken in "
                     "jouw track - dus je loop kan nooit leeg raken. Maak je het vak leeg met het "
                     "kruisje, dan gaat KEY weer terug naar wat je had."),
        ("In sync", "De loop en je eigen track lopen altijd samen: zelfde tempo, zelfde toonsoort, "
                    "zelfde tel. In je DAW wint het tempo van de DAW (het tempoveld zegt SYNC) en "
                    "begint maat 1 van de loop op maat 1 van je song. Zonder DAW-tempo leidt je "
                    "eigen track: het veld zegt TRACK en volgt het MY TRACK-vak."),
        ("Stukje kiezen", "Over elke golfvorm - in de acht slots en in het MY TRACK-vak - staan "
                          "twee lijntjes. Sleep ze naar het stuk dat je wilt gebruiken; alles "
                          "daarbuiten wordt donker en telt niet meer mee. Dubbelklik op de "
                          "golfvorm om weer het hele sample te pakken. Het afspeelknopje speelt "
                          "precies dat stuk, met een meelopende streep."),
        ("AUTO PICK", "Maakt acht loops, geeft ze punten en houdt de beste over. KEEP zet de loop "
                      "in de eerstvolgende vrije scene, zodat je verder kunt zoeken."),
        ("Energy", "Knop in het Character-paneel: hoe verder in de loop, hoe voller en glitchiger."),
        ("Accent", "Knop in het Character-paneel: de stukjes op de tel worden luider dan die "
                   "ertussen, zodat een 1/16-roll ademt in plaats van ratelt. Alle vier de tellen "
                   "krijgen evenveel accent en de loop wordt er niet harder van."),
        ("Slice size", "Hoe lang een stukje maximaal mag zijn (1/32 tot 1 bar). Een langere noot "
                       "in het ritme wordt opgebouwd uit meerdere stukjes van die maat, elk uit "
                       "een ander plekje in je samples - 4 to the floor met slice size 1/32 geeft "
                       "dus echt 32 stukjes in een maat."),
        ("Stems", "In EXPORT...: per sample een eigen WAV van dezelfde loop, om zelf te mixen."),
        ("Slepen", "Onderaan twee tegels: WAV (de loop als audio, al op het tempo van je song en "
                   "precies op een maat) en MIDI (per stukje een noot). EXPORT... maakt ook een "
                   "WAV, MIDI-bestand of een slice kit (elk stukje een eigen WAV + de MIDI)."),
        ("MIDI NOTES", "Control = noten bedienen de plugin. Slices = C1 speelt de hele loop, C#1 "
                       "en hoger elk stukje apart (samen met de MIDI-tegel). Keys = de loop speelt "
                       "in de toonsoort die je aanslaat (C3 = origineel)."),
        ("Tempo &amp; toonsoort", "Staat het niet in de bestandsnaam, dan luistert Chupa Loops "
                                  "naar de audio zelf (nooit een toonsoort voor drumloops). De "
                                  "losse app begint op 125 BPM (je eigen tempo wordt met de sessie "
                                  "bewaard), en KEY begint daar op Off zodat er niks ongemerkt "
                                  "getransponeerd wordt - tenzij er iets in het MY TRACK-vak zit, "
                                  "dan komt die toonsoort gewoon terug."),
        ("Vastzetten", "Rechtsklik op een stukje in het resultaat om het vast te zetten. Het "
                       "blijft precies klinken zoals je het op dat moment hoorde, ook na NEW LOOP "
                       "en MUTATE. Klikken op een stukje vervangt alleen dat ene stukje - ook het "
                       "laatste."),
        ("Terug", "Met de pijltjes komt een versie terug mét de instellingen, dus ook na de "
                  "gekke knop. Eerste keer openen: een korte rondleiding (ook via ?)."),
        ("MIDI", "C1 = nieuwe loop, C#1/D1 = vorige/volgende versie, D#1 = alles ontgrendelen, E1 "
                 "= gekke knop, F1 = mutate, C2 t/m G#2 = ritme, C3 t/m F3 = lengte, C4 t/m G4 = "
                 "scenes, program change = preset."),
        ("MIDI learn", "Rechtsklik op een knop of keuzeveld &gt; MIDI learn en draai aan je "
                       "controller."),
        ("Automatiseren", "De parameter New Loop maakt een nieuwe loop bij elke stap van uit naar "
                          "aan."),
        ("About", "Knop ?: MIDI-overzicht, versie en CHECK FOR UPDATES (kijkt op je "
                  "GitHub-releasepagina)."),
        ("Installers", "Echte installers: .pkg in een .dmg voor Mac, Setup.exe voor Windows."),
    ], left=104))

    s.append(PageBreak())
    s.append(M.H("Deel D: later - ondertekenen (fase 2)"))
    s.append(M.P("Alles staat al klaar in het project. Zodra je de certificaten hebt, zet je ze "
                 "als Secrets in GitHub (Settings &gt; Secrets and variables &gt; Actions &gt; New "
                 "repository secret). De volgende build ondertekent dan automatisch en laat de "
                 "Mac-installer notariseren door Apple. Welke secrets precies nodig zijn, staat in "
                 "de README op GitHub."))
    s.append(M.steps([
        "<b>Apple:</b> Apple Developer Program (99 euro per jaar). Maak daar een <b>Developer ID "
        "Application</b>- en een <b>Developer ID Installer</b>-certificaat en een app-specifiek "
        "wachtwoord voor notarisatie.",
        "<b>Windows:</b> nieuwe certificaten kun je niet meer als bestand exporteren. Gebruik "
        "daarom <b>Azure Trusted Signing</b> van Microsoft (goedkoop, per maand); het project "
        "ondersteunt dat al. Kijk bij het aanmelden of het voor jouw situatie (bedrijf/zzp in "
        "Nederland) beschikbaar is.",
    ]))
    s.append(Spacer(1, 4 * mm))
    s.append(M.note("Let op de naam: “Chupa” lijkt op het merk Chupa Chups. De plugin "
                    "gebruikt daarom een eigen lolly-ontwerp en geen logo of vormgeving van dat "
                    "merk. Laat de naam checken (bijv. bij het Benelux-Bureau voor de "
                    "Intellectuele Eigendom of een merkenadviseur) voordat je gaat verkopen."))
    return s


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else M.DOCS
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, "Stappenplan Chupa Loops.pdf")

    M.FOOTER = "Stappenplan Chupa Loops %s" % M.VERSION
    doc = M.Manual(out)
    doc.title = "Stappenplan Chupa Loops"
    # this one has no cover page: every page gets the normal frame
    doc.pageTemplates[0].onPage = M.draw_page
    doc.build(build_story())
    print("written:", out, os.path.getsize(out), "bytes")


if __name__ == "__main__":
    sys.exit(main())
