#!/usr/bin/env python3
"""Render the Sydrahynth editor UI to an editable SVG.

Replicates plugin/PanelView.cpp's paint code (plus the HydraEditor status bar
and PatchBrowser chrome) so the generated document is a faithful, 1:1
rendition of the app's layout. Everything lives in semantic <g> groups with
ids and palette classes so it can be restyled in Figma/Illustrator/Inkscape
and the changes mapped back onto the code.

Usage: tools/render-panel-svg.py > docs/sydrahynth-ui.svg
"""

import math
import re

W, H = 2230, 1036          # document: browser (270) + bar (36) + panel canvas
CW, CH = 1960, 1000        # PanelView::kCanvasWidth x kCanvasHeight
BAR = 36                   # HydraEditor::kBarHeight
BROWSER = 270              # HydraEditor::kBrowserWidth

# Palette (PanelView.cpp / PatchBrowser.cpp / HydraEditor.cpp)
C = {
    "bg": "#1b1d21", "panel": "#26292e", "label": "#b8bcc4", "led": "#ffd27a",
    "orange": "#e0702a", "btn": "#3a3e45", "btn_held": "#4a4f58",
    "knob": "#111215", "knob_ring": "#50555e", "enc_ring": "#5a5f68",
    "led_lit": "#6fd3ff", "led_unlit": "#32363c", "pad": "#2e3238",
    "pad_edge": "#4a4f58", "heading": "#1e2024", "leader": "#7d828b",
    "sigline": "#9a9fa8", "bezel": "#000000", "screen": "#05070a",
    "chrome": "#1e2126", "browse_bg": "#15171a", "browse_sel": "#3a4a6a",
    "browse_text": "#d8dce3", "browse_dim": "#6a6f78", "browse_edge": "#2a2e35",
    "widget": "#4a4f58",
}

FONT = "Helvetica Neue, Arial, sans-serif"
out = []


def emit(s):
    out.append(s)


def rect(x, y, w, h, rx=0, fill=None, stroke=None, sw=0, id=None, cls=None, opacity=None):
    a = f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}"'
    if rx:
        a += f' rx="{rx}"'
    if fill:
        a += f' fill="{fill}"'
    if stroke:
        a += f' stroke="{stroke}" stroke-width="{sw}"'
    if id:
        a += f' id="{id}"'
    if cls:
        a += f' class="{cls}"'
    if opacity:
        a += f' opacity="{opacity}"'
    emit(a + '/>')


def circle(cx, cy, r, fill=None, stroke=None, sw=0, id=None, cls=None, opacity=None):
    a = f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="{r:.2f}"'
    if fill:
        a += f' fill="{fill}"'
    if stroke:
        a += f' stroke="{stroke}" stroke-width="{sw}"'
    if id:
        a += f' id="{id}"'
    if cls:
        a += f' class="{cls}"'
    if opacity:
        a += f' opacity="{opacity}"'
    emit(a + '/>')


def ellipse(cx, cy, r, stroke=None, sw=0, fill="none", id=None, opacity=None):
    circle(cx, cy, r, fill=fill, stroke=stroke, sw=sw, id=id, opacity=opacity)


def line(x1, y1, x2, y2, stroke, sw, dash=None, id=None):
    a = f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" stroke="{stroke}" stroke-width="{sw}"'
    if dash:
        a += f' stroke-dasharray="{dash}"'
    if id:
        a += f' id="{id}"'
    emit(a + '/>')


def text(x, y, s, size, fill, anchor="middle", bold=False, id=None, cls=None):
    w = ' font-weight="bold"' if bold else ''
    i = f' id="{id}"' if id else ''
    c = f' class="{cls}"' if cls else ''
    s = (s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))
    emit(f'<text x="{x:.2f}" y="{y:.2f}" font-family="{FONT}" font-size="{size}"{w} '
         f'fill="{fill}" text-anchor="{anchor}" dominant-baseline="central"{i}{c}>{s}</text>')


def circ_point(cx, cy, r, angle):
    # juce::Point::getPointOnCircumference: 0 = up, clockwise.
    return cx + r * math.sin(angle), cy - r * math.cos(angle)


# ---------------------------------------------------------------- buttons ---
# label, keyscan id, led index, centre x/y, w, h (defaults 70x36)
BUTTONS = [
    ("ON", 26, 30, 385, 412, 70, 36), ("LATCH", 28, 31, 532, 412, 70, 36),
    ("TRIPLET", 46, 32, 680, 412, 70, 36), ("TAP TEMPO", 48, 29, 827, 412, 70, 36),
    ("FILTER 1", 79, 26, 802, 562, 70, 36), ("FILTER 2", 80, 27, 802, 630, 70, 36),
    ("BROWSE", 45, 34, 955, 124, 76, 36), ("SAVE", 50, 33, 955, 238, 76, 36),
    ("SYSTEM", 27, 38, 955, 314, 76, 36), ("INIT", 44, 35, 955, 418, 76, 36),
    ("RANDOM", 59, 36, 955, 488, 76, 36), ("SHIFT", 62, 39, 955, 598, 76, 36),
    ("<", 51, -1, 1065, 488, 62, 36), (">", 55, -1, 1175, 488, 62, 36),
    ("HOME", 49, -1, 1120, 598, 170, 40),
    ("EXIT", 7, 45, 1870, 124, 76, 36), ("VOICE", 56, 46, 1870, 234, 76, 36),
    ("UP", 60, 47, 1870, 316, 76, 36), ("DOWN", 61, 48, 1870, 378, 76, 36),
    ("MACRO ASSIGN", 57, 49, 1870, 486, 76, 36), ("MOD MATRIX", 58, 58, 1870, 596, 76, 36),
    ("", 33, 50, 1315, 235, 62, 30), ("", 34, 51, 1455, 235, 62, 30),
    ("", 35, 52, 1597, 235, 62, 30), ("", 36, 53, 1737, 235, 62, 30),
    ("", 37, 54, 1315, 488, 62, 30), ("", 38, 55, 1455, 488, 62, 30),
    ("", 39, 56, 1597, 488, 62, 30), ("", 40, 57, 1737, 488, 62, 30),
    ("OSC 1", 11, 0, 125, 770, 70, 36), ("MUTANT 1", 14, 3, 230, 770, 78, 36),
    ("MUTANT 2", 15, 4, 332, 770, 78, 36), ("OSC 2", 12, 1, 125, 838, 70, 36),
    ("MUTANT 3", 16, 5, 230, 838, 78, 36), ("MUTANT 4", 3, 6, 332, 838, 78, 36),
    ("OSC 3", 13, 2, 125, 908, 70, 36), ("RING-NOISE", 17, 7, 332, 908, 78, 36),
    ("MIXER", 0, 8, 437, 838, 70, 36), ("FILTER 1", 24, 19, 540, 805, 70, 36),
    ("FILTER 2", 25, 20, 540, 872, 70, 36), ("AMP", 1, 21, 644, 838, 70, 36),
    ("PRE-FX", 2, 22, 746, 838, 70, 36), ("DELAY", 4, 23, 851, 805, 70, 36),
    ("REVERB", 5, 24, 851, 872, 70, 36), ("POST-FX", 6, 25, 953, 838, 70, 36),
    ("ENV 1", 70, 9, 540, 728, 70, 36), ("ENV 2", 71, 10, 644, 728, 70, 36),
    ("ENV 3", 72, 11, 748, 728, 70, 36), ("ENV 4", 73, 12, 850, 728, 70, 36),
    ("ENV 5", 18, 13, 955, 728, 70, 36),
    ("LFO 1", 66, 14, 540, 948, 70, 36), ("LFO 2", 67, 15, 644, 948, 70, 36),
    ("LFO 3", 68, 16, 748, 948, 70, 36), ("LFO 4", 69, 17, 850, 948, 70, 36),
    ("LFO 5", 47, 18, 955, 948, 70, 36),
]
SHIFT_LEGENDS = {26: "EDIT", 28: "SUSTAIN", 45: "FAVORITE", 49: "PANIC", 51: "-10", 55: "+10"}


def bnd(b):
    return b[3] - b[5] / 2, b[4] - b[6] / 2, b[5], b[6]


def module_box(bid):
    for b in BUTTONS:
        if b[1] == bid and b[4] > 700:
            return bnd(b)
    return None


# ----------------------------------------------------------------- panel ---
def paint_panel():
    emit(f'<g id="panel-canvas" transform="translate({BROWSER} {BAR})">')
    rect(0, 0, CW, CH, fill=C["bg"], id="background")
    rect(20, 20, CW - 40, CH - 40, rx=16, fill=C["panel"], id="panel-face")

    text(70 + 8, 70 + 30, "SYDRAHYNTH", 40, C["label"], anchor="start", bold=True, id="brand")
    # (centredLeft: x at box left edge; small optical inset kept for the mock)

    # Section headings
    emit('<g id="headings">')
    for t, x, y, w in [("ARPEGGIATOR", 606, 200, 560), ("FILTER CONTROLS", 611, 490, 470),
                       ("MODULE SELECT", 230, 678, 300), ("MAIN SYSTEMS", 1065, 30, 300),
                       ("MASTER CONTROL", 1590, 30, 600)]:
        emit(f'<g id="heading-{t.replace(" ", "-")}">')
        rect(x - w / 2, y, w, 26, rx=4, fill=C["heading"])
        text(x, y + 13, t, 20, C["orange"], bold=True)
        emit('</g>')
    emit('</g>')

    paint_module_lines()
    paint_displays()
    paint_buttons()
    paint_knobs()
    paint_encoders()
    paint_pads()
    emit('</g>')


def paint_displays():
    emit('<g id="displays">')
    # Sample OLED content approximating a live patch page (the emulator fills
    # these with the firmware's 128x64 framebuffer)
    sample = [("DeltaStringer RA", "A001"), ("Strings", "85.8"), ("Wave 2", "0.0"),
              ("Envelope", "98.1"), ("Delay", "50.0")]
    for i, (x, y, w, h) in enumerate([(1030, 318, 180, 90), (1247, 325, 135, 68),
                                      (1388, 325, 135, 68), (1529, 325, 135, 68),
                                      (1670, 325, 135, 68)]):
        emit(f'<g id="display-{i}">')
        rect(x - 4, y - 4, w + 8, h + 8, fill=C["bezel"])
        rect(x, y, w, h, fill=C["screen"], id=f"display-{i}-screen")
        name, value = sample[i]
        text(x + w / 2, y + h * 0.35, name, 18 if i == 0 else 15, "#d8dce3")
        text(x + w / 2, y + h * 0.7, value, 13, C["browse_dim"])
        emit('</g>')
    emit('</g>')


def paint_buttons():
    emit('<g id="buttons">')
    for i, (label, bid, led, cx, cy, w, h) in enumerate(BUTTONS):
        x, y = bnd((None, None, None, cx, cy, w, h))[0:2]
        name = re.sub(r"[^A-Za-z0-9_-]+", "-", label) or f"press-{bid - 32}"
        emit(f'<g id="btn-{name}-{bid}">')
        rect(x, y, w, h, rx=5, fill=C["btn"], cls="button-face")
        if led >= 0:
            # LED strip slot (shown dim; PanelView lights it with the level)
            rect(x + 3, y + h - 3 - 5, w - 6, 5, rx=2, fill=C["led"], cls="button-led", opacity="0.35")
        if label:
            size = 10.0 if (w < 72 and len(label) > 7) else 12.0
            text(cx, y + (h - 5) / 2, label, size, C["label"])
        if bid in SHIFT_LEGENDS:
            text(cx, y + h + 3 + 7, SHIFT_LEGENDS[bid], 11, C["orange"], bold=True, cls="shift-legend")
        if 33 <= bid <= 40:
            n = bid - 32
            ty = y + h + 3 + 7 if n <= 4 else y - 17 + 7
            text(cx, ty, str(n), 12, C["label"], cls="encoder-number")
        if bid == 61:
            text(cx, y + h + 4 + 7, "PAGE", 12, C["label"], bold=True)
        emit('</g>')
    emit('</g>')


def paint_module_lines():
    emit(f'<g id="module-lines" stroke="{C["sigline"]}">')
    K = {"osc1": 11, "osc2": 12, "osc3": 13, "mut1": 14, "mut2": 15, "mut3": 16, "mut4": 3,
         "ring": 17, "mixer": 0, "f1": 24, "f2": 25, "amp": 1, "prefx": 2, "delay": 4,
         "reverb": 5, "postfx": 6, "env1": 70, "env2": 71, "env5": 18,
         "lfo1": 66, "lfo2": 67, "lfo5": 47}

    def box(k):
        return module_box(K[k])

    # Orange rails behind the ENV and LFO rows
    emit('</g><g id="modulator-rails">')
    for top, last in (("env1", "env5"), ("lfo1", "lfo5")):
        y = box(top)[1] + box(top)[3] / 2
        line(box(top)[0] - 45, y, box(last)[0] + box(last)[2] + 35, y, C["orange"], 4)
    emit('</g><g id="signal-flow">')

    def ln(a, b, c, d, dashed=False):
        if dashed:
            line(a, b, c, d, C["sigline"], 2, dash="6 5")
        else:
            line(a, b, c, d, C["sigline"], 2)

    cy = lambda b: b[1] + b[3] / 2
    cx = lambda b: b[0] + b[2] / 2
    xr = lambda b: b[0] + b[2]
    xl = lambda b: b[0]
    yt = lambda b: b[1]
    yb = lambda b: b[1] + b[3]

    for a, b in (("osc1", "mut1"), ("mut1", "mut2"), ("osc2", "mut3"), ("mut3", "mut4"), ("osc3", "ring")):
        ln(xr(box(a)), cy(box(a)), xl(box(b)), cy(box(a)))
    mixBus = (xr(box("mut2")) + xl(box("mixer"))) / 2
    for i in ("mut2", "mut4", "ring"):
        ln(xr(box(i)), cy(box(i)), mixBus, cy(box(i)))
    ln(mixBus, cy(box("mut2")), mixBus, cy(box("ring")))
    ln(mixBus, cy(box("mixer")), xl(box("mixer")), cy(box("mixer")))

    inBus = (xr(box("mixer")) + xl(box("f1"))) / 2
    outBus = (xr(box("f1")) + xl(box("amp"))) / 2
    ln(xr(box("mixer")), cy(box("mixer")), inBus, cy(box("mixer")))
    ln(outBus, cy(box("amp")), xl(box("amp")), cy(box("amp")))
    for bus, side in ((inBus, "l"), (outBus, "r")):
        ln(bus, cy(box("f1")), bus, cy(box("f2")))
        for f in ("f1", "f2"):
            ln(bus, cy(box(f)), xl(box(f)) if side == "l" else xr(box(f)), cy(box(f)))
    ln(cx(box("f1")), yb(box("f1")), cx(box("f1")), yt(box("f2")))

    for top, mt, mb, bot in (("env1", "f1", "f2", "lfo1"), ("env2", "amp", "amp", "lfo2")):
        x = cx(box(mt))
        ln(x, yb(box(top)), x, yt(box(mt)), dashed=True)
        ln(x, yb(box(mb)), x, yt(box(bot)), dashed=True)

    ln(xr(box("amp")), cy(box("amp")), xl(box("prefx")), cy(box("prefx")))
    fxIn = (xr(box("prefx")) + xl(box("delay"))) / 2
    fxOut = (xr(box("delay")) + xl(box("postfx"))) / 2
    ln(xr(box("prefx")), cy(box("prefx")), fxIn, cy(box("prefx")))
    ln(fxOut, cy(box("postfx")), xl(box("postfx")), cy(box("postfx")))
    for bus, side in ((fxIn, "l"), (fxOut, "r")):
        ln(bus, cy(box("delay")), bus, cy(box("reverb")))
        for f in ("delay", "reverb"):
            ln(bus, cy(box(f)), xl(box(f)) if side == "l" else xr(box(f)), cy(box(f)))
    ln(cx(box("delay")), yb(box("delay")), cx(box("delay")), yt(box("reverb")))
    emit('</g>')


ARP_LEGENDS = [
    ["UP", "DOWN", "UP / DN", "UP & DN", "ORDER", "RANDOM", "CHORD", "PHRASE"],
    ["", "", "", "", "1", "2", "3", "4"],
    ["1/4", "1/8", "1/16", "1/32", "", "", "", ""],
    ["OFF", "52%", "54%", "58%", "62%", "66%", "70%", "75%"],
]

# kind, label, switch index, x, y, radius
KNOBS = [
    ("volume", "MASTER VOLUME", 0, 175, 405, 38),
    ("arp", "MODE", 0, 385, 300, 27), ("arp", "OCTAVE", 1, 532, 300, 27),
    ("arp", "DIVISION", 2, 680, 300, 27), ("arp", "SWING", 3, 827, 300, 27),
    ("analog", "CUTOFF", 0, 422, 600, 34), ("analog", "RESONANCE", 1, 547, 600, 34),
    ("analog", "DRIVE / MORPH", 2, 673, 600, 34),
]


def paint_knobs():
    emit('<g id="knobs">')
    for kind, label, idx, x, y, r in KNOBS:
        emit(f'<g id="knob-{label.replace(" ", "-").replace("/", "-")}">')
        circle(x, y, r, fill=C["knob"], stroke=C["knob_ring"], sw=2)
        if kind == "arp":
            slot = idx if idx != 1 else 4      # arpPositions_ all start at 0; OCTAVE's slot is +4
        else:
            slot = 3.5                          # value 0.5 -> straight up
        angle = math.pi * (-0.75 + 1.5 * slot / 7.0)
        px, py = circ_point(x, y, r - 4, angle)
        line(x, y, px, py, "#ffffff", 3)
        if kind == "arp":
            paint_arp_legends(x, y, r, idx)
        if kind == "volume":
            text(x, y - r - 14, label, 12, C["label"])
            text(x, y + r + 14, "13.0 dB", 12, C["label"])
        else:
            text(x, y + r + 16, label, 13, C["label"], bold=True)
        emit('</g>')
    emit('</g>')


def paint_arp_legends(x, y, r, idx):
    ROW = 12.5
    emit('<g class="arp-legends">')
    for slot in range(8):
        legend = ARP_LEGENDS[idx][slot]
        if not legend:
            continue
        left = slot < 4
        ly = y + (1.5 - slot) * ROW if left else y + (slot - 5.5) * ROW
        side = -1.0 if left else 1.0
        angle = math.pi * (-0.75 + 1.5 * slot / 7.0)
        fx, fy = circ_point(x, y, r + 3, angle)
        ex, ey = x + side * (r + 12), ly
        tx = x + side * (r + 18)
        line(fx, fy, ex, ey, C["leader"], 1.2)
        line(ex, ey, tx, ly, C["leader"], 1.2)
        anchor = "end" if left else "start"
        text(tx + (side * 2), ly, legend, 9.5, C["label"], anchor=anchor, bold=True)
    emit('</g>')


ENC_X = [1315, 1455, 1597, 1737]
ENC_Y = [122, 597]
ENC_R = 23
DATA_X, DATA_Y, DATA_R = 1120, 155, 76


def paint_encoders():
    emit('<g id="encoders">')
    for e in range(8):
        cx, cy = ENC_X[e & 3], ENC_Y[e >> 2]
        emit(f'<g id="encoder-{e + 1}">')
        circle(cx, cy, ENC_R, fill=C["knob"], stroke=C["enc_ring"], sw=2)
        emit(f'<g id="ring-{e + 1}">')
        for l in range(15):
            angle = math.pi * (-0.75 + 1.5 * l / 14.0)
            px, py = circ_point(cx, cy, ENC_R + 10, angle)
            lit = l <= 8  # sample state for the mock: ~60 %
            circle(px, py, 3, fill=C["led_lit"] if lit else C["led_unlit"],
                   cls="led-lit" if lit else "led-unlit")
        emit('</g></g>')
    emit('</g>')

    emit('<g id="data-encoder">')
    ring = "#d93a5e"  # WS2812 patch ring in the loaded patch's colour
    ellipse(DATA_X, DATA_Y, DATA_R + 9 + 5, stroke=ring, sw=6, opacity="0.25", id="patch-ring-glow")
    ellipse(DATA_X, DATA_Y, DATA_R + 9, stroke=ring, sw=9, id="patch-ring")
    circle(DATA_X, DATA_Y, DATA_R, fill=C["knob"], stroke=C["enc_ring"], sw=2)
    text(DATA_X, DATA_Y, "PATCH", 12, C["label"])
    emit('</g>')


PAD_W, PAD_H, PAD_X, PAD_PITCH = 82, 62, 1056, 108.2
PAD_Y = [921, 828, 733]
PAD_LEGENDS = [
    ["Chromatic", "Major", "Maj Pent", "Min Pent", "Blues", "Bebop", "Whole Tone", "Dim Whole"],
    ["Dorian", "Phrygian", "Lydian", "Mixolydian", "Minor", "Locrian", "Harmonic Min", "Melodic Min"],
    ["OCT -", "OCT +", "PAD MODE -", "PAD MODE +", "KEY", "", "", ""],
]
# Sample pad colours approximating a live patch (the firmware sets these per patch)
PAD_MOCK = {}
for c in range(8):
    PAD_MOCK[(2, c)] = "#4a6f96" if c < 2 or c > 4 else "#a04838"
    PAD_MOCK[(1, c)] = "#4a6f96" if c in (1, 3, 6, 7) else ("#b8a038" if c == 4 else "#a04838")
    PAD_MOCK[(0, c)] = "#b8a038" if c == 0 else ("#a04838" if c in (1, 3) else "#4a6f96")


def paint_pads():
    emit('<g id="pads">')
    for row in range(3):
        for col in range(8):
            x = PAD_X + PAD_PITCH * col
            y = PAD_Y[row] - PAD_H / 2
            fill = PAD_MOCK.get((row, col), C["pad"])
            emit(f'<g id="pad-r{row}c{col}">')
            rect(x, y, PAD_W, PAD_H, rx=6, fill=fill, stroke=C["pad_edge"], sw=1.5)
            legend = PAD_LEGENDS[row][col]
            if legend:
                if legend == "KEY":
                    box = x + PAD_W / 2 - 18, y + PAD_H + 3
                    rect(box[0], box[1], 36, 14, rx=2, fill=C["orange"])
                    text(x + PAD_W / 2, box[1] + 7, legend, 11, "#1b1d21", bold=True)
                else:
                    text(x + PAD_W / 2, y + PAD_H + 3 + 7, legend, 11, C["orange"], bold=True, cls="pad-legend")
            emit('</g>')
    emit('</g>')


# ---------------------------------------------------------------- chrome ---
def paint_status_bar():
    emit(f'<g id="status-bar" transform="translate({BROWSER} 0)">')
    rect(0, 0, CW, BAR, fill=C["chrome"])
    # resized(): bar = removeFromTop(36).reduced(8,4)
    y, h = 4, 28
    text(8 + 4, y + h / 2 + 1, "Sydrahynth", 18, C["browse_text"], anchor="start", bold=True, id="title")
    rect(138, y, 80, h, rx=5, fill=C["widget"], id="btn-Patches")
    text(178, y + h / 2 + 1, "Patches", 13, "#f0f0f0")
    # right side: firmware btn 100, 8, latency 140, 8, status 300, then label
    rect(1852, y, 100, h, rx=5, fill=C["widget"], id="btn-Firmware")
    text(1902, y + h / 2 + 1, "Firmware...", 13, "#f0f0f0")
    rect(1704, y, 140, h, rx=5, fill=C["widget"], id="combo-latency")
    text(1716, y + h / 2 + 1, "Latency 12 ms", 13, "#f0f0f0", anchor="start")
    text(1690, y + h / 2 + 1, "Hydrasynth Desktop 2.2.0 · running", 13, C["browse_dim"], anchor="end", id="status-text")
    emit('</g>')


def paint_browser():
    emit('<g id="patch-browser">')
    rect(0, BAR, BROWSER, CH, fill=C["chrome"])
    ax, ay, aw = 8, BAR + 6, BROWSER - 16
    text(ax + 2, ay + 12, "Patches", 15, C["browse_text"], anchor="start", bold=True)
    ay += 24 + 4
    bw = aw / 5
    for b in range(5):
        rect(ax + b * bw, ay, bw, 26, rx=0 if b else 4,
             fill=C["browse_sel"] if b == 0 else C["widget"], id=f"bank-{'ABCDE'[b]}")
        text(ax + b * bw + bw / 2, ay + 13, "ABCDE"[b], 13, "#f0f0f0")
    ay += 26 + 6
    # list: bottom "Load bank..." 26 + 6 gap
    list_y, list_h = ay, BAR + CH - 6 - 26 - 6 - ay
    rect(ax, list_y, aw, list_h, fill=C["browse_bg"], stroke=C["browse_edge"], sw=1, id="patch-list")
    names = ["DeltaStringer RA", "OmniPluk DN", "Space Demon NS", "Swimming Bass NS",
             "MeloLead DN", "Insomnia RA", "Wounded Knee RA", "NeoSoulHorns NS",
             "Round'n'Low NS", "Jazz Guitar NS", "TapeStrings DN", "Troposphere RA",
             "Light Feather RA", "Hollow Pluck DN", "Fingers LR", "P-Bass & B3 RA",
             "Cinematic NS", "Brass d'Air RA", "Sunny CP 70 RA", "Suitcase Wow! RA"]
    rows = list_h // 22
    for r in range(rows):
        ry = list_y + r * 22
        rect(ax, ry, aw, 22, fill=C["browse_sel"] if r == 0 else (C["browse_bg"] if r % 2 == 0 else C["chrome"]))
        name = names[r] if r < len(names) else "(empty)"
        col = C["browse_text"] if r < len(names) else C["browse_dim"]
        text(ax + 8, ry + 11, f"A{r + 1:03d}", 14, col, anchor="start")
        text(ax + 54, ry + 11, name, 14, col, anchor="start")
    ay = BAR + CH - 6 - 26
    rect(ax, ay, aw, 26, rx=5, fill=C["widget"], id="btn-LoadBank")
    text(ax + aw / 2, ay + 13, "Load bank...", 13, "#f0f0f0")
    emit('</g>')


emit('<?xml version="1.0" encoding="UTF-8"?>')
emit(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">')
emit(f'<desc>Sydrahynth UI rendition, generated by tools/render-panel-svg.py from '
     f'plugin/PanelView.cpp + HydraEditor.cpp + PatchBrowser.cpp. '
     f'1 document unit = 1 canvas px; the panel is {CW}x{CH}.</desc>')
rect(0, 0, W, H, fill=C["chrome"], id="app-background")
paint_browser()
paint_status_bar()
paint_panel()
emit('</svg>')
print("\n".join(out))
