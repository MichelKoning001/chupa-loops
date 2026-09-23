#!/usr/bin/env python3
"""Builds docs/Chupa Loops Manual.pdf.

    python3 tools/make_manual.py

Screenshots come from build/test_output (run the UI test first: cd build &&
xvfb-run -a ./UITest_artefacts/Release/UITest), with docs/ as a fallback.
Keep this file in step with the plug-in: it is the only source of the manual.
"""

import os
import sys
from PIL import Image, ImageDraw, ImageFont

from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_LEFT
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph,
                                Spacer, Table, TableStyle, Image as RLImage,
                                KeepTogether, PageBreak, Flowable, NextPageTemplate)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS = os.path.join(ROOT, "Resources", "fonts")
DOCS = os.path.join(ROOT, "docs")
SHOTS = os.path.join(ROOT, "build", "test_output")
TMP = os.path.join(ROOT, "build", "manual_tmp")
VERSION = "1.1"
FOOTER = "Chupa Loops %s – User manual" % VERSION   # the stappenplan overrides this

PINK = colors.HexColor("#FF2E88")
DARK = colors.HexColor("#2B1533")
GREY = colors.HexColor("#6B5A72")
LINE = colors.HexColor("#F3D9E6")
SOFT = colors.HexColor("#FFF4F9")


def shot(name):
    """Prefer a freshly rendered screenshot, fall back to the one in docs/."""
    for folder in (SHOTS, DOCS):
        p = os.path.join(folder, name)
        if os.path.exists(p):
            return p
    raise SystemExit("missing screenshot: " + name)


# ---------------------------------------------------------------- fonts
pdfmetrics.registerFont(TTFont("Erica", os.path.join(FONTS, "EricaOne-Regular.ttf")))
pdfmetrics.registerFont(TTFont("Inter", os.path.join(FONTS, "Inter-Regular.ttf")))
pdfmetrics.registerFont(TTFont("Inter-Bold", os.path.join(FONTS, "Inter-Bold.ttf")))
pdfmetrics.registerFontFamily("Inter", normal="Inter", bold="Inter-Bold",
                              italic="Inter", boldItalic="Inter-Bold")

body = ParagraphStyle("body", fontName="Inter", fontSize=9.2, leading=13.4,
                      textColor=DARK, alignment=TA_LEFT, spaceAfter=5)
small = ParagraphStyle("small", parent=body, fontSize=7.8, leading=11, textColor=GREY)
cell = ParagraphStyle("cell", parent=body, fontSize=8.6, leading=12, spaceAfter=0)
term = ParagraphStyle("term", parent=cell, fontName="Inter-Bold", textColor=PINK)
num = ParagraphStyle("num", parent=cell, fontName="Inter-Bold", textColor=DARK)
head = ParagraphStyle("head", fontName="Erica", fontSize=16, leading=20,
                      textColor=PINK, spaceBefore=13, spaceAfter=6, keepWithNext=1)
sub = ParagraphStyle("sub", fontName="Inter-Bold", fontSize=10, leading=14,
                     textColor=DARK, spaceBefore=8, spaceAfter=3)
caption = ParagraphStyle("caption", parent=small, spaceBefore=4, spaceAfter=10)


# ---------------------------------------------------------------- helpers
def P(text, style=body):
    return Paragraph(text, style)


def H(text):
    return Paragraph(text, head)


def defs(rows, left=104, style=term):
    """A two-column term/description table, as used all through the manual."""
    data = [[Paragraph(a, style), Paragraph(b, cell)] for a, b in rows]
    t = Table(data, colWidths=[left, None], hAlign="LEFT")
    t.setStyle(TableStyle([
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("TOPPADDING", (0, 0), (-1, -1), 4),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
        ("LEFTPADDING", (0, 0), (0, -1), 0),
        ("RIGHTPADDING", (0, 0), (0, -1), 8),
        ("LEFTPADDING", (1, 0), (1, -1), 0),
        ("LINEBELOW", (0, 0), (-1, -2), 0.4, LINE),
    ]))
    return t


def steps(items):
    """The numbered quick-start list."""
    data = [[Paragraph(str(i + 1), num), Paragraph(t, cell)] for i, t in enumerate(items)]
    t = Table(data, colWidths=[16, None], hAlign="LEFT")
    t.setStyle(TableStyle([
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("TOPPADDING", (0, 0), (-1, -1), 4),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
        ("LEFTPADDING", (0, 0), (-1, -1), 0),
        ("RIGHTPADDING", (0, 0), (0, -1), 6),
    ]))
    return t


def note(text):
    """A tinted footnote box."""
    t = Table([[Paragraph(text, small)]], colWidths=[None], hAlign="LEFT")
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), SOFT),
        ("TOPPADDING", (0, 0), (-1, -1), 6),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 6),
        ("LEFTPADDING", (0, 0), (-1, -1), 8),
        ("RIGHTPADDING", (0, 0), (-1, -1), 8),
        ("LINEBEFORE", (0, 0), (0, -1), 2, PINK),
    ]))
    return t


def picture(path, width, cap=None, align="LEFT"):
    im = Image.open(path)
    h = width * im.height / im.width
    out = [RLImage(path, width=width, height=h, hAlign=align)]
    if cap:
        out.append(Paragraph(cap, caption))
    return out


class Rule(Flowable):
    def __init__(self, width):
        Flowable.__init__(self)
        self.width, self.height = width, 1

    def draw(self):
        self.canv.setStrokeColor(LINE)
        self.canv.setLineWidth(1)
        self.canv.line(0, 0, self.width, 0)


# ---------------------------------------------------------------- figures
def stripes(w, h, a="#FFD9E8", b="#FFEFF6"):
    """The candy-stripe background of the cover."""
    im = Image.new("RGB", (w, h), b)
    d = ImageDraw.Draw(im)
    step = 34
    for x in range(-h, w + h, step * 2):
        d.polygon([(x, 0), (x + step, 0), (x + step + h, h), (x + h, h)], fill=a)
    return im


def make_cover_bg(path):
    stripes(1240, 1754).save(path)


def make_numbered(path):
    """The interface screenshot with twelve numbered callouts in a clean margin,
    so no number ever covers a control."""
    im = Image.open(shot("screenshot_2x.png")).convert("RGB")
    s = im.width / 1120.0           # the editor is laid out on 1120 x 800
    band = int(40 * s)              # the margin the numbers live in
    out = Image.new("RGB", (im.width + band, im.height + 2 * band), (255, 255, 255))
    out.paste(im, (band, band))
    d = ImageDraw.Draw(out)
    try:
        f = ImageFont.truetype(os.path.join(FONTS, "Inter-Bold.ttf"), int(21 * s))
    except OSError:
        f = ImageFont.load_default()

    # (number, centre in the 1120 x 800 layout; the band is added around it)
    left = [(1, 30), (5, 82), (6, 148), (7, 400), (8, 565), (9, 700)]
    top = [(2, 470), (3, 798), (4, 1046)]
    bottom = [(10, 424), (11, 740), (12, 1015)]
    r = 15 * s
    half = band / 2.0

    def circle(cx, cy, n):
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(43, 21, 51))
        t = str(n)
        bb = d.textbbox((0, 0), t, font=f)
        d.text((cx - (bb[2] - bb[0]) / 2 - bb[0], cy - (bb[3] - bb[1]) / 2 - bb[1]),
               t, font=f, fill=(255, 255, 255))

    for n, y in left:
        circle(half, band + y * s, n)
    for n, x in top:
        circle(band + x * s, half, n)
    for n, x in bottom:
        circle(band + x * s, out.height - half, n)
    out.save(path)


def make_trim(path):
    """Slot 1 and the MY TRACK box, both with their two lines dragged in."""
    im = Image.open(shot("screenshot_trim.png")).convert("RGB")
    s = im.width / 1120.0
    def crop(x0, y0, x1, y1):
        return im.crop((int(x0 * s), int(y0 * s), int(x1 * s), int(y1 * s)))
    a = crop(14, 104, 288, 194)
    b = crop(14, 518, 288, 612)
    w = max(a.width, b.width)
    gap = int(14 * s)
    out = Image.new("RGB", (w, a.height + gap + b.height), (255, 255, 255))
    out.paste(a, (0, 0))
    out.paste(b, (0, a.height + gap))
    out.save(path)


# ---------------------------------------------------------------- page frame
PAGE_W, PAGE_H = A4
MARGIN = 20 * mm
CONTENT_W = PAGE_W - 2 * MARGIN


def draw_cover(canv, doc):
    canv.saveState()
    canv.drawImage(os.path.join(TMP, "cover_bg.png"), 0, 0, PAGE_W, PAGE_H)
    canv.restoreState()


def draw_page(canv, doc):
    canv.saveState()
    canv.setFont("Inter", 7.5)
    canv.setFillColor(GREY)
    canv.drawString(MARGIN, 12 * mm, FOOTER)
    canv.drawRightString(PAGE_W - MARGIN, 12 * mm, str(doc.page))
    canv.setStrokeColor(LINE)
    canv.setLineWidth(0.5)
    canv.line(MARGIN, 15 * mm, PAGE_W - MARGIN, 15 * mm)
    canv.restoreState()


class Manual(BaseDocTemplate):
    def __init__(self, path):
        BaseDocTemplate.__init__(self, path, pagesize=A4,
                                 leftMargin=MARGIN, rightMargin=MARGIN,
                                 topMargin=18 * mm, bottomMargin=20 * mm,
                                 title="Chupa Loops - User manual", author="Percep-tion",
                                 subject="Loop slicer & loop generator")
        frame = Frame(MARGIN, 20 * mm, CONTENT_W, PAGE_H - 38 * mm, id="body",
                      leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
        self.addPageTemplates([
            PageTemplate(id="cover", frames=[frame], onPage=draw_cover),
            PageTemplate(id="body", frames=[frame], onPage=draw_page),
        ])


# ---------------------------------------------------------------- content
def build_story():
    s = []

    # ------------------------------------------------ cover
    s.append(Spacer(1, 26 * mm))
    icon = os.path.join(ROOT, "Resources", "icon.png")
    s.append(RLImage(icon, width=34 * mm, height=34 * mm, hAlign="CENTER"))
    s.append(Spacer(1, 8 * mm))
    s.append(Paragraph('<font color="#2B1533">Chupa</font> <font color="#FF2E88">Loops</font>',
                       ParagraphStyle("title", fontName="Erica", fontSize=40, leading=44,
                                      alignment=TA_LEFT)))
    s.append(Paragraph("Loop slicer &amp; loop generator \u2013 User manual",
                       ParagraphStyle("tsub", fontName="Inter", fontSize=13, leading=18,
                                      textColor=GREY, spaceBefore=4)))
    s.append(Spacer(1, 6 * mm))
    s += picture(shot("screenshot.png"), CONTENT_W)
    s.append(Paragraph("Version %s \u2013 VST3, Audio Unit and standalone app for macOS and "
                       "Windows \u2013 by Percep-tion" % VERSION, small))
    s.append(NextPageTemplate("body"))
    s.append(PageBreak())

    # ------------------------------------------------ welcome
    s.append(H("Welcome"))
    s.append(P("Chupa Loops turns the loops you already have into new ones. Feed it up to 8 loops "
               "\u2013 basslines work great \u2013 and it cuts them into slices taken from anywhere "
               "in those loops, then puts the slices back together on the rhythm you choose. Every "
               "press of NEW LOOP gives you a fresh loop, in the tempo and the key of your song. "
               "Keep pressing until you love it, then drag it onto a track."))

    s.append(H("Install it"))
    s.append(defs([
        ("Mac", "Open ChupaLoops-macOS.dmg and double-click Chupa Loops Installer.pkg. It installs "
                "the VST3, the Audio Unit and the app (click Customize to choose). If macOS says "
                "it can't check the installer, go to System Settings &gt; Privacy &amp; Security "
                "and click Open Anyway."),
        ("Windows", "Run ChupaLoops-Windows-Setup.exe. It installs the VST3 in C:\\Program "
                    "Files\\Common Files\\VST3 and the app in Program Files. If SmartScreen warns "
                    "you, click More info &gt; Run anyway."),
        ("Manual install", "The .zip downloads contain the plug-ins as files, for copying by hand."),
        ("Standalone app", "The app works without a DAW. Set your audio device under Options. "
                           "Shortcuts: Space play/stop, N new loop, Cmd/Ctrl+Z previous version, "
                           "Shift+Cmd/Ctrl+Z next version, [ ] previous/next preset."),
    ]))
    s.append(note("Restart your DAW and rescan your plug-ins after installing. Chupa Loops is an "
                  "<b>instrument</b>, not an effect: look under Percep-tion &gt; Chupa Loops. "
                  "Updates: open ? and click CHECK FOR UPDATES \u2013 that button is the only "
                  "moment Chupa Loops goes online. A short tour opens the first time; ? &gt; SHOW "
                  "THE TOUR opens it again."))

    s.append(H("Quick start"))
    s.append(steps([
        "Add Chupa Loops to an instrument track in your DAW (it is listed under instruments: "
        "Percep-tion &gt; Chupa Loops). Or open the standalone app.",
        "Drag 2 to 8 loops onto the slots \u2013 from Finder/Explorer, Splice or your DAW's "
        "browser. You can drop several files at once. Tempo and key are read from the file names, "
        "or heard in the audio when the name doesn't say.",
        "Pick a rhythm (for example Offbeat or Rolling KBBB) and a length, or choose a preset from "
        "the preset bar at the top. Working on your own track? Drop a part of it in the MY TRACK "
        "box under the result: the new loop then fits around it.",
        "Press play in your DAW (or PREVIEW) and hit NEW LOOP until you hear something you like "
        "\u2013 or press AUTO PICK and let Chupa Loops make eight loops and keep the best one. "
        "<b>You do not need a MIDI clip on the track:</b> Chupa Loops plays its loop by itself as "
        "soon as your DAW plays. MIDI notes are only there to control it, or to play the slices "
        "(see \u201cPlay it as an instrument\u201d).",
        "Click a slice to swap just that slice. Right-click a slice to lock it, so it stays when "
        "you make the next loop. MUTATE gives you the same loop again, but a bit different. KEEP "
        "parks a loop in a scene (A \u2013 H).",
        "Drag the loop out with WAV (audio) or MIDI (one note per slice), or use EXPORT... for a "
        "WAV, a MIDI file, a slice kit or stems.",
    ]))

    s.append(PageBreak())
    s.append(H("The interface"))
    s += picture(os.path.join(TMP, "numbered.png"), 0.90 * CONTENT_W, align="CENTER")
    s.append(defs([
        ("1 Mascot", "Your skin's mascot. It reacts every time a new loop is made."),
        ("2 Presets", "\u25c0 \u25b6 step through the presets, click the name to open the preset "
                      "browser, SAVE stores your own. An asterisk (*) means you changed something "
                      "since loading the preset."),
        ("3 Tempo &amp; preview",
         "The tempo the loop really runs at. SYNC = from your DAW, TRACK = from the sample in the "
         "MY TRACK box. Without either it starts on 125 BPM; drag or double-click to set it. "
         "PREVIEW plays the loop without starting your DAW; in the standalone app it reads PLAY."),
        ("4 Skin &amp; ?", "SKIN changes the look. ? opens the About screen: MIDI chart, version "
                           "and update check."),
        ("5 Samples bar", "Scenes A \u2013 H, KEY match, the SPLICE button and CLEAR ALL. CLEAR "
                          "ALL is a fresh start: click it twice and it empties all eight slots and "
                          "the MY TRACK box, and puts the settings, the knobs, the KEY and the "
                          "preset back to Init. Only your output Volume stays where you put it."),
        ("6 Slots", "Eight sample slots: play button, name, STR, share (%), tempo, level, "
                    "transpose and on/off, and two lines over the waveform to use only a part. "
                    "See \u201cSamples\u201d below."),
        ("7 Result", "The new loop: every coloured block is a slice, coloured by the slot it comes "
                     "from. AUTO PICK and KEEP sit in its title row."),
        ("8 My track", "Drop a part of the song you are working on here: FIT TO TRACK (see below)."),
        ("9 Rhythm", "Where the slices land in the bar, and the phrase FILL."),
        ("10 Loop &amp; slicing", "Length, repeat, slice mode, stretch mode, slice size and MIDI "
                                  "NOTES (Control / Slices / Keys)."),
        ("11 Character | FX", "Style (Clean / Glitch / Lo-Fi) and eleven knobs (including ACCENT "
                              "and ENERGY); the FX tab holds the finishing effects and the volume."),
        ("12 Actions", "NEW LOOP, MUTATE, the switch that decides whether NEW LOOP resets the "
                       "panel, the skin's crazy button (see below), the version history (◀ "
                       "▶ with “3 / 8” between them: version 3 of the 8 you made), "
                       "unlock, EXPORT... and the WAV / MIDI drag tiles."),
    ], left=92))

    # ------------------------------------------------ samples
    s.append(PageBreak())
    s.append(H("Samples"))
    s.append(P("Drop a file on a slot, or click an empty slot to browse. WAV, AIFF, FLAC, MP3 and "
               "OGG are supported. Dropping several files fills the next free slots. Dropping on a "
               "loaded slot replaces it. From samples longer than about two minutes only the first "
               "two minutes are used – your file on disk is never changed."))
    s.append(defs([
        ("BPM field", "The tempo of the sample, detected from the file name (Splice style: "
                      "<b>128bpm</b>, <b>_140_</b>), or from the loop length together with the "
                      "beat it hears in the audio. Drag up/down to correct it (Shift = fine), "
                      "double-click to go back to automatic."),
        ("dB field", "The level of this sample in the loop, \u201324 to +24 dB. Handy when a "
                     "mastered loop sits next to a raw recording, or when one sample keeps jumping "
                     "out of the loop. Listening to the sample on its own uses this level too. "
                     "Drag up/down (Shift = fine), double-click to type a value (0 and \u20136 are "
                     "both fine), right-click for 0 dB."),
        ("st field", "Transpose this slot by \u201312 to +12 semitones."),
        ("STR", "Straighten: puts a sloppy or human recording back on the grid \u2013 an old "
                "record, a live take, a vocal that drifts. Chupa Loops hears the beats, works out "
                "the even grid they belong on, and stretches the bit between two beats to fit. "
                "Click to switch it on, drag up/down for how far: 100% is dead straight, 60% takes "
                "the wobble out and keeps the feel. Double-click = off. This is about the timing "
                "<i>inside</i> one sample; STRETCH in the panel is something else \u2013 that fits "
                "a sample recorded at another tempo to your song's tempo."),
        ("ON / OFF", "Take this slot out of the mix without unloading it."),
        ("Play button", "Listen to this sample on its own, at its own tempo, looping. Click again "
                        "to stop; it also stops when your DAW plays."),
        ("The two lines", "Drag the two lines over the waveform to use only a part of a sample: "
                          "slices are then only taken from between them, and everything outside "
                          "them goes dark. Double-click the waveform (or right-click &gt; Use the "
                          "whole sample again) to reset them. The play button also plays just that "
                          "part."),
        ("Right-click", "The slot menu: listen to the sample, use the whole sample again (undo the "
                        "two lines), set <b>Stretch this sample</b> (see below), or clear the slot."),
        ("Stretch per slot", "Every slot can ignore the STRETCH setting in the panel. Follow the "
                             "panel is the normal setting; Beats keeps the attacks exactly as "
                             "recorded (drums, bass); Smooth time-stretches and keeps the pitch "
                             "(vocals, pads). A slot that is not following the panel shows a small "
                             "BEATS or SMOOTH tag next to its key tag."),
        ("Share (%)", "How often slices are taken from this sample: 100% is a normal share, 0% "
                      "means never, 200% twice as often. Drag up/down, double-click = 100%."),
        ("Tags", "The key detected from the name (for example Am), the key it is moved to by KEY "
                 "match (Am \u2192 F#m), how much the loop is stretched (+9%), and BEATS or SMOOTH "
                 "when this slot has its own stretch mode."),
        ("KEY", "Key match: every sample with a known key is moved to the chosen key or its "
                "relative major/minor (Am fits C). Off = no change. When the name has no key, "
                "Chupa Loops listens to the audio; it only sets a key when it is sure, and never "
                "for drum loops (names with kick, hats, drums, perc, top loop ...).<br/>"
                "What is remembered: a project in your DAW, and a state file you load by hand, "
                "come back with exactly the key they were saved with. The standalone app always "
                "starts on Off, so nothing is transposed without you asking \u2013 unless a track "
                "is still in the MY TRACK box, and then its key is used again."),
        ("SPLICE", "Opens Splice in your browser. Drag sounds from the Splice app, the Splice "
                   "Sounds plug-in or the Splice tab in your DAW's browser onto a slot. Tempo and "
                   "key are read from the Splice file name. If Splice already matched a sound to "
                   "your song's tempo, Chupa Loops notices that from its length. If Splice also "
                   "matched the key, set KEY to Off so the sound isn't transposed twice."),
    ]))
    s.append(note("Samples up to 64 seconds long are saved inside your project (compressed, "
                  "lossless), so a project still opens when a file has moved."))

    s.append(Spacer(1, 5 * mm))
    s += picture(os.path.join(TMP, "trim.png"), 78 * mm,
                 "The two lines on a sample slot and on the MY TRACK box. Everything outside them "
                 "is dark: no slices are taken from there. On the track box they also pick the "
                 "part Chupa Loops listens to when it works out where your track is busy.")

    # ------------------------------------------------ fit to track
    s.append(H("Your own track"))
    s.append(defs([
        ("What to drop in", "One or two bars of the track you are working on is enough, and it "
                            "works best when it holds the parts the new loop has to stay out of "
                            "the way of – usually your kick, bass and main groove. Bounce "
                            "that section from your DAW as a WAV and drag it in, or drag the clip "
                            "straight from your DAW's browser. Dropped something longer? Use the "
                            "two lines on its waveform to pick the bar you mean."),
        ("FIT TO TRACK", "Drop a part of the track you are working on into the MY TRACK box, in "
                         "the left column under the result. That sample is never sliced. Chupa "
                         "Loops measures where your track is busy inside a bar and leaves those "
                         "spots free, so the new loop fits around it instead of on top of it. The "
                         "KEY also follows that sample. The % field in the box is the strength: it "
                         "starts at 100% = normal, 200% = really stay out of the way, and at 0% "
                         "the loop stops keeping out of the way (the key keeps following your "
                         "track until you empty the box). The two lines on its waveform pick the "
                         "part it listens to. Empty the box (the x) to switch fitting off again."),
        ("Always in sync", "The loop and your track always run together: same tempo, same key, "
                           "same beat. In a DAW the DAW's tempo wins \u2013 you are making a track "
                           "at that tempo, so the loop and your track both follow it, and bar 1 of "
                           "the loop lands on bar 1 of your song. Without a DAW tempo (the "
                           "standalone app) your own track leads: its tempo becomes the loop's "
                           "tempo and the tempo field reads TRACK. Press play on the track box and "
                           "the loop above it comes in on the beat, so you can hear straight away "
                           "whether they fit."),
        ("BPM in the box", "The tempo the fit grid is built on. Correct it here if the track's "
                           "tempo was read wrong; in the standalone app it is also the tempo "
                           "everything else runs at."),
        ("It never empties your loop",
         "Whatever you do, slices are always kept: about a quarter of the bar at 100%, fewer as "
         "you turn FIT up, and always at least one. The ones it keeps are the slices that sit on "
         "the quietest spots in your track. So even when your track and the rhythm you picked land "
         "on exactly the same beats, you still get a loop. An even sound without a clear rhythm (a "
         "pad, a drone), or a sample shorter than one bar, gives nothing to fit around: then the "
         "loop simply stays as it was."),
    ]))

    # ------------------------------------------------ rhythm
    s.append(H("Rhythm, loop &amp; slicing"))
    s.append(defs([
        ("Rhythm", "Where the slices land in the bar:<br/>"
                   "<b>Free</b> – slices back to back, each one as long as the slice size.<br/>"
                   "<b>4 to the floor</b> – one quarter note on every beat.<br/>"
                   "<b>Offbeat</b> – one eighth note between every beat: the classic offbeat "
                   "bass.<br/>"
                   "<b>Offbeat 2x</b> – the same idea twice as fast: two 16ths between every "
                   "beat.<br/>"
                   "<b>Rolling 16th</b> – a steady run of 16ths.<br/>"
                   "<b>Rolling KBBB</b> – three 16ths after every beat and nothing on the "
                   "beat itself (kick-bass-bass-bass): the beat is left free for your kick. The "
                   "hard trance and hardstyle bassline.<br/>"
                   "<b>Gallop</b> – an eighth on the beat, then two 16ths: a galloping "
                   "push.<br/>"
                   "<b>Broken</b> – a fixed syncopated pattern, off the beat, breakbeat-"
                   "like.<br/>"
                   "<b>Random</b> – new positions every time, on the 1/16 grid, with notes of "
                   "a 16th, an eighth or three 16ths."),
        ("Length", "1, 2, 4, 8, 16 or 32 bars."),
        ("Repeat", "The same 1, 2 or 4 bar pattern comes back through the whole loop, so it stays "
                   "musical instead of wandering. Off = every bar is new."),
        ("Slice mode", "Grid = cut on the beat grid. Transient = cut at every new note."),
        ("Stretch", "How loops at another tempo are fitted. Beats (best for basslines) re-slices "
                    "on the grid, so attacks stay exactly as recorded. Smooth time-stretches (best "
                    "for long notes and pads). A single slot can be set apart from this in its "
                    "right-click menu."),
        ("Slice size", "How long one slice may be at most, from 1/32 to 1 bar. In Free mode it is "
                       "simply the length of every slice. In the other rhythms, a note longer than "
                       "this is built from several slices of this size in a row, each one from a "
                       "different place in your samples – so 4 to the floor with a slice size "
                       "of 1/32 really gives you 32 slices in a bar. A note that is already "
                       "shorter stays as it is. Smaller = more chopped, more variation; bigger = "
                       "longer, more recognisable pieces."),
        ("Fill", "Off, 4, 8, 16 or 32 bars: the last half bar of every phrase becomes a stutter "
                 "roll that gets faster, like a drummer's fill. A loop shorter than the fill "
                 "length gets its fill at the end of the loop."),
        ("MIDI notes", "What incoming MIDI notes do: Control (the chart below), Slices or Keys "
                       "\u2013 see \u201cPlay it as an instrument\u201d."),
    ]))
    s.append(note("Want a sample at half or double speed? Set its BPM field to half or double the "
                  "tempo it shows. That is the same move for one slot, instead of for all of them "
                  "at once."))

    # ------------------------------------------------ result
    s.append(H("Result"))
    s.append(defs([
        ("Click a slice", "Replaces just that slice with a new one. Every slice can be clicked, "
                          "including the last one in the loop."),
        ("Right-click a slice", "Locks / unlocks it (Alt-click, Cmd-click on Mac or Ctrl-click on "
                                "Windows works too). Locking freezes exactly what you are hearing: "
                                "that slice keeps its sound through NEW LOOP and MUTATE."),
        ("Markers", "Lock icon = locked, R = reversed, G = glitch, +12 = one octave up."),
        ("\u25c0 \u25b6 and 3 / 8", "Step back to earlier versions of the loop, and forward again. "
                                    "The number between the arrows is the version you are on out "
                                    "of the ones you made (up to 200 per session). A version comes "
                                    "back with its settings, so the crazy button can always be "
                                    "undone. Your FX moves stay."),
        ("MUTATE", "A variation of this loop: about a quarter of the unlocked slices change, the "
                   "rest stays. Press it again for another variation."),
        ("NEW LOOP knobs", "The switch under MUTATE decides what NEW LOOP does with the CHARACTER "
                           "| FX panel. <b>KEEP KNOBS</b> (normal) leaves it exactly as you set "
                           "it. <b>ALL NEUTRAL</b> puts that panel back to neutral first – "
                           "the style, all eleven character knobs and all the FX – so every "
                           "new loop starts from a clean sheet. What the loop is made of stays "
                           "yours either way: rhythm, length, repeat, slicing, FILL and Volume are "
                           "never touched. Click the switch to change it."),
        ("AUTO PICK", "Makes eight loops, gives them a score \u2013 enough going on, punchy, "
                      "nicely spread over your samples, varied \u2013 and keeps the best one."),
        ("KEEP", "Parks this loop in the first free scene, so you can keep looking without losing "
                 "it."),
    ]))

    s.append(H("Scenes A \u2013 H"))
    s.append(P("Scenes keep your favourite loops ready to play live. Click an empty scene to store "
               "the current loop (with all its settings and FX), click a stored scene to bring it "
               "back. Shift-click stores (and replaces), right-click gives Store / Recall / Clear. "
               "The lit scene is the one you hear; it goes dark as soon as you change the loop. "
               "Scenes are saved with your project and can be played with MIDI notes C4 \u2013 G4."))

    # ------------------------------------------------ character
    s.append(PageBreak())
    s.append(H("Character"))
    s.append(defs([
        ("Style", "Clean: the joins between slices are inaudible. Glitch: stutters, tape stops and "
                  "chops. Lo-Fi: a crunchy vintage sampler."),
        ("Chaos", "How freely a slice may move away from the spot it came from. Low: a slice that "
                  "sat on beat 2 in the original lands on a beat-2 position in the new loop, so "
                  "the groove of your samples stays tight. High: any slice can land anywhere "
                  "– more surprising, less predictable."),
        ("Variation", "With Repeat on: how many slices change in each repeat of the pattern."),
        ("Gate", "Note length. Lower = shorter and stabbier."),
        ("Accent", "Makes the slices that land on the beat louder than the ones in between, so a "
                   "1/16 roll breathes instead of rattling. 0% = every slice the same. All four "
                   "beats of the bar get the same accent, so the groove stays in 4/4 and never "
                   "starts to feel like a 3-count. Turning Accent up does not make the loop louder "
                   "– it only changes the balance inside the bar."),
        ("Swing", "Pushes every second step back for a shuffle. The step it works on is the slice "
                  "size, so swing on 1/16 shuffles 16ths and swing on 1/8 shuffles eighths (1/2 "
                  "is as slow as the shuffle goes)."),
        ("Amount", "Strength of Glitch or Lo-Fi."),
        ("Reverse / Octave", "Chance that a slice plays backwards / one octave up."),
        ("Fade", "Crossfade between slices: short = punchy, long = smooth."),
        ("Sens", "Transient detection sensitivity (Transient mode)."),
        ("Energy", "Builds the loop up: the further into the loop, the more rolls, octaves and "
                   "reverses. 0% = the same from start to end."),
        ("Volume", "Output level (in the FX tab), \u201336 to +12 dB. A transparent limiter keeps "
                   "the sliced loop at \u20130.3 dBFS, before Volume and the FX."),
    ]))
    s.append(note("Every knob: Shift-drag = fine, double-click = default, right-click = MIDI "
                  "learn. The window can be resized by dragging its corner (75% to 200%); the size "
                  "is remembered with your project."))

    s.append(H("FX"))
    s.append(P("The FX tab is the finishing touch on the whole loop \u2013 it is not a per-step "
               "effect sequencer. The effects are in the playback, in exports and in the drag "
               "tiles."))
    s.append(defs([
        ("Cutoff / Reso", "Low-pass filter over the loop and its resonance. 100% = open."),
        ("Env / Decay", "Opens the filter on every slice and lets it close again over the Decay "
                        "time: every chop gets its own \u201cwow\u201d (use Cutoff below 100%)."),
        ("Low cut", "Removes low end, to make room for your kick and bass."),
        ("Drive", "Warm saturation up to distortion."),
        ("Pump", "Sidechain pump on every beat, in time with your song."),
        ("Width", "Stereo width: 0% mono, 100% unchanged, 200% extra wide."),
    ]))
    s.append(note("Presets from the factory don't touch the FX and Fill, so you can browse presets "
                  "with your finishing layer on. ENERGY and ACCENT do go back to 0% with a factory "
                  "preset. Init resets everything; your own presets store the FX too."))

    # ------------------------------------------------ into your song
    s.append(H("Into your song"))
    s.append(defs([
        ("WAV tile", "Drag the loop as audio onto an audio track. The WAV is already at your "
                     "song's tempo and starts exactly on a bar, so drop it on a bar line and "
                     "leave warping off (or set it to 1 bar) – nothing needs stretching. The "
                     "file is named after what is in it, for example <i>Chupa Loops 140bpm 4bars "
                     "Offbeat.wav</i>, and a copy is kept in Music/Chupa Loops (MIDI drags land in "
                     "MIDI, kits in Kits, stems in Stems)."),
        ("MIDI tile", "Drag the loop as MIDI onto the Chupa Loops track: one note per slice. Set "
                      "MIDI NOTES to Slices and the notes play the loop back exactly. Now move, "
                      "delete or repeat notes to re-arrange your loop in the piano roll."),
        ("EXPORT...", "Loop as WAV (24-bit, or 32-bit float when Volume pushes the loop above 0 "
                      "dBFS), Loop as MIDI, a slice kit: a folder in Music/Chupa Loops/Kits with "
                      "the full loop, every different slice as its own WAV (named after its key) "
                      "and the MIDI file \u2013 ready for any sampler. Stems writes one WAV per "
                      "sample (the same loop, but only the slices of that sample), so you can mix "
                      "them yourself. Stems are dry: Lo-Fi and the limiter only run on the mix."),
    ]))

    # ------------------------------------------------ instrument
    s.append(H("Play it as an instrument"))
    s.append(defs([
        ("Slices", "C1 plays the whole loop, C#1 and up play every different slice on its own key "
                   "(a slice that repeats in the loop shares its key). Up to 91 different slices "
                   "fit on the keyboard (C#1 up to G8); with more, use Repeat, a bigger slice size "
                   "or a shorter loop. Velocity sets the level."),
        ("Keys", "Hold a key and the loop plays in that key, in time with your song: C3 = original "
                 "pitch, up to two octaves up or down. Play a bassline or chords with your new "
                 "loop."),
        ("Control", "The standard mode: notes control Chupa Loops (see MIDI control)."),
    ]))

    s.append(H("Presets"))
    s.append(P("Chupa Loops comes with 100 presets in ten flavours. A preset sets the rhythm, "
               "length, slicing, style and character \u2013 never your samples, the key or the "
               "volume \u2013 so you can flip through presets with the same loops loaded."))
    flav = [("Trance Treats", "Trance"), ("Hard Candy", "Hard trance &amp; hardhouse"),
            ("Psy Sweets", "Psytrance"), ("Techno Toffee", "Techno"),
            ("House Candy", "House &amp; garage"), ("Breakbeat Brittle", "Breaks &amp; drum and bass"),
            ("Glitch Gummies", "Glitch &amp; IDM"), ("Lo-Fi Liquorice", "Lo-fi"),
            ("Rave Candy", "Rave classics"), ("Pick 'n' Mix", "Experimental")]
    s.append(defs(flav, left=120))
    s.append(Spacer(1, 3 * mm))
    s.append(P("Click the preset name to open the browser: pick a flavour on the left, search by "
               "name or style, click a preset to hear it (the browser stays open) and double-click "
               "to load it and close. SAVE (or SAVE CURRENT AS... in the browser) stores your own "
               "preset in Music/Chupa Loops/Presets (the browser has an OPEN PRESET FOLDER and a "
               "DELETE button, an All category, and the search box also matches the style). "
               "Presets are small files (.chupapreset): copy them to another computer or send them "
               "to a friend, and drop one on the plug-in window to import it. The standalone app "
               "also has [ and ] for the previous / next preset. Your DAW sees the factory presets "
               "as programs."))

    # ------------------------------------------------ skins
    s.append(PageBreak())
    s.append(H("Skins"))
    s.append(P("Click SKIN to pimp your Chupa. Your choice is remembered for every project and "
               "every instance. Lolly and Fruity share the swirl lolly in their own colours; "
               "Skull, Butcher, Neon, Acid and Smile each have their own: a skull lolly, a lolly "
               "with a steel cleaver chopping into it, a neon lolly, an acid lolly with a bubbling "
               "flask and a smiley lolly. The Butcher even chops the name: it reads Chopa Loops."))
    s += picture(shot("skins.png"), CONTENT_W,
                 "Top: Lolly, Fruity, Skull, Butcher. Bottom: Neon, Acid, Smile.")

    s.append(H("The craziest loop ever"))
    s.append(P("In the actions column, under the NEW LOOP knob switch, sits the skin's crazy "
               "button. It throws the rhythm, slicing and "
               "character around in the style of the skin and makes a new loop straight away. Your "
               "samples, length, key, stretch mode and volume are never touched. Liked it? Save it "
               "as a preset. Too much? Load your preset again to calm down, or step back with "
               "\u25c0."))
    s.append(defs([
        ("SUGAR RUSH (Lolly)", "Maximum chaos, octave jumps and stutters on rolling rhythms."),
        ("FRUIT PUNCH (Fruity)", "Bouncy and swung, with lots of octave jumps."),
        ("SKULL DAMAGE (Skull)", "Crushed Lo-Fi, backwards slices, broken rhythms."),
        ("THE BUTCHER CUT (Butcher)", "Tiny, stabby chops on every transient."),
        ("NEON OVERDRIVE (Neon)", "Rolling and glitchy, lit up with octaves."),
        ("ACID FLASHBACK (Acid)", "Smeared, backwards and swung, with Glitch or Lo-Fi."),
        ("SMILEY MAYHEM (Smile)", "Anything goes: every setting at random."),
    ], left=140))

    # ------------------------------------------------ midi
    s.append(PageBreak())
    s.append(H("MIDI control"))
    s.append(P("Send MIDI to the Chupa Loops track (play notes on a keyboard, or draw them in a "
               "MIDI clip) to control it live. These notes work with MIDI NOTES set to Control; in "
               "Slices and Keys the notes play the loop instead."))
    s.append(defs([
        ("C1 (note 36)", "New loop"),
        ("C#1 / D1", "Previous / next version"),
        ("D#1", "Unlock all slices"),
        ("E1", "The skin's crazy button"),
        ("F1", "Mutate"),
        ("C2 \u2013 G#2", "Rhythm: C2 Free, C#2 4 to the floor, D2 Offbeat, D#2 Offbeat 2x, E2 "
                          "Rolling 16th, F2 Rolling KBBB, F#2 Gallop, G2 Broken, G#2 Random"),
        ("C3 \u2013 F3", "Length: C3 1 bar, C#3 2, D3 4, D#3 8, E3 16, F3 32 bars"),
        ("C4 \u2013 G4", "Scenes A \u2013 H"),
        ("Program change", "0 = Init, 1 \u2013 100 = the factory presets"),
    ], left=92))
    s.append(note("Note names use middle C = C3 (note 60), as in Studio Pro, Logic, Cubase and "
                  "Ableton Live. Other DAWs may call note 36 \u201cC2\u201d (or \u201cC3\u201d in "
                  "FL Studio): it is always the lowest C of the chart, note number 36."))

    s.append(H("MIDI learn"))
    s.append(P("Right-click any knob, any selector in the panels or the NEW LOOP button and choose "
               "MIDI learn, then "
               "move a knob or fader on your controller. A small CC tag shows the mapping. "
               "Right-click again and choose Forget to remove it. Mappings are saved with your "
               "project. Learn is cancelled by clicking anywhere, by pressing Escape, or after 20 "
               "seconds."))

    s.append(H("Automation"))
    s.append(P("Every knob, selector and button in the panels can be automated (the fields on the "
               "sample slots – tempo, dB, st, share, STR – belong to the sample and are "
               "saved with your project, but are not automation targets). The extra New Loop "
               "parameter makes a new loop "
               "every time it goes from off to on: draw it as short steps in an automation lane to "
               "get a new loop on, say, every 8th bar. When you export or bounce, automated "
               "settings are followed exactly. New loops made by MIDI notes or the New Loop "
               "parameter are made the moment they arrive, so in a fast offline bounce they can "
               "land a little later than in real time: for an exact result, record the loops you "
               "like with the WAV tile."))

    s.append(H("Working in your DAW"))
    s.append(defs([
        ("Fender Studio Pro", "Drag Chupa Loops from the Browser (Instruments) onto the "
                              "arrangement. It follows the song tempo; press play. Drag your loops "
                              "and Splice samples straight from the Browser onto the slots."),
        ("Logic Pro", "Create a Software Instrument track and choose AU Instruments &gt; "
                      "Percep-tion &gt; Chupa Loops."),
        ("Ableton Live", "Plug-ins &gt; VST3 (or Audio Units) &gt; Percep-tion &gt; Chupa Loops, "
                         "on a MIDI track. Drag the loop from the WAV tile into an audio track or "
                         "Session slot."),
        ("Cubase", "Add an Instrument Track with Chupa Loops (Percep-tion, category Sampler)."),
        ("FL Studio", "Scan for plug-ins in the Plugin Manager, then add Chupa Loops to the "
                      "Channel Rack."),
    ]))
    s.append(note("When exporting or bouncing, Chupa Loops waits until the loop is ready, so the "
                  "bounce always contains the final loop. The loop runs on your DAW's tempo and "
                  "starts on the bar, so it stays locked to your song from the first beat."))

    # ------------------------------------------------ troubleshooting
    s.append(PageBreak())
    s.append(H("Troubleshooting"))
    s.append(defs([
        ("I can't find the plug-in", "Restart your DAW and rescan plug-ins. Chupa Loops is an "
                                     "instrument, not an effect."),
        ("No sound", "Press play in your DAW or click PREVIEW. Check that at least one slot is ON "
                     "and Volume is up. You don't need a MIDI clip on the track: Chupa Loops plays "
                     "on its own."),
        ("Wrong tempo for a sample", "Drag the BPM field of that slot to the right tempo "
                                     "(double-click = automatic again)."),
        ("One sample is too loud", "Drag its dB field down, or double-click it and type a value. "
                                   "Right-click puts it back to 0 dB."),
        ("The loop doesn't line up with my track",
         "In a DAW the loop follows the DAW's tempo: check that your track and the tempo field "
         "show the same number, and that the sample in the MY TRACK box has the right BPM. In the "
         "standalone app the tempo field reads TRACK and follows the track box."),
        ("MIDI notes do nothing / play sound", "Check MIDI NOTES: Control = the notes control "
                                               "Chupa Loops, Slices and Keys = the notes play the "
                                               "loop."),
        ("The MIDI clip sounds wrong", "Set MIDI NOTES to Slices, and use the clip with the loop "
                                       "it was dragged from (a new loop has other slices)."),
        ("Loops clash harmonically", "Set KEY, or transpose single slots with the st field."),
        ("The loop sounds empty", "Check the slots: a share of 0% means that sample is never used, "
                                  "two lines close together leave almost nothing to cut from, and "
                                  "a high FIT leaves a lot of room for your track \u2013 turn the "
                                  "% in the MY TRACK box down, or empty the box."),
        ("AUTO PICK does nothing", "It needs samples and a loop, and it takes a moment: it makes "
                                   "eight loops first. The result line tells you the score."),
        ("Joins sound soft", "Use Stretch: Beats (on the panel or on that one slot), lower Fade, "
                             "or raise Gate."),
        ("A file is red: \u201cFile not found\u201d",
         "The file has moved and there was no copy inside the project to fall back on \u2013 that "
         "happens with samples longer than 64 seconds, which are too long to store. Click the slot "
         "to locate the file again. Slots under 64 seconds simply keep playing from the copy in "
         "your project and never turn red."),
        ("Typing in the preset name does nothing", "Some DAWs keep the keyboard for themselves. "
                                                   "Click inside the text field first, or allow "
                                                   "keyboard input for plug-ins in your DAW."),
    ], left=126))

    s.append(Spacer(1, 6 * mm))
    s.append(Rule(CONTENT_W))
    s.append(Spacer(1, 3 * mm))
    s.append(P("<b>Credits</b>  Chupa Loops by Alexander Koning (Percep-tion). Built with JUCE. "
               "Time-stretching: Signalsmith Stretch (MIT license). Fonts: Inter, Erica One, "
               "Boldonse, Tektur and Big Shoulders (SIL Open Font License). VST is a trademark of "
               "Steinberg Media Technologies GmbH. Audio Unit and Logic Pro are trademarks of "
               "Apple Inc. Splice is a trademark of Splice. All other names belong to their "
               "owners.", small))
    return s


def main():
    os.makedirs(TMP, exist_ok=True)
    make_cover_bg(os.path.join(TMP, "cover_bg.png"))
    make_numbered(os.path.join(TMP, "numbered.png"))
    make_trim(os.path.join(TMP, "trim.png"))

    out = os.path.join(DOCS, "Chupa Loops Manual.pdf")
    doc = Manual(out)
    story = build_story()
    doc.build(story)
    print("written:", out, os.path.getsize(out), "bytes")


if __name__ == "__main__":
    sys.exit(main())
