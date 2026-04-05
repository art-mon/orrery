"""
orrery — documentation schematic generator (schemdraw 0.22)
Outputs a single docs/schematic.svg with four sections:
  1. Power distribution
  2. HUB75 interface
  3. Audio interface
  4. Button circuits
"""

import schemdraw
import schemdraw.elements as elm
from schemdraw.util import Point
from pathlib import Path
import matplotlib
import matplotlib.pyplot as plt
matplotlib.use("Agg")

OUT = Path(__file__).parent

# ─── layout constants ─────────────────────────────────────────────────────────
# schemdraw y increases upward; sections are stacked bottom→top

SEC_BUTTONS_Y  = 0       # bottom section (left: audio, right: buttons)
SEC_AUDIO_Y    = 0
SEC_HUB75_Y    = 7       # middle section
SEC_POWER_Y    = 22      # top section
TITLE_Y        = 26.5

AUDIO_X        = 0
BUTTONS_X      = 12

HUB75_PINS = [
    ('R1','4'),('G1','5'),('B1','6'),
    ('R2','7'),('G2','15'),('B2','16'),
    ('A','36'),('B','37'),('C','38'),('D','39'),
    ('CLK','2'),('LAT','47'),('OE','48'),
]
N = len(HUB75_PINS)      # 13


# ─── drawing ──────────────────────────────────────────────────────────────────
with schemdraw.Drawing(show=False) as d:
    d.config(fontsize=8.5, inches_per_unit=0.52)

    # ── section title helper ──────────────────────────────────────────────────
    def section_title(text, x, y):
        d.add(elm.Label().at(Point((x, y))).label(
            f'— {text} —', loc='center').color('#555555'))

    # ─── TITLE ───────────────────────────────────────────────────────────────
    d.add(elm.Label().at(Point((9, TITLE_Y + 0.8)))
          .label('orrery — Hardware Schematic', loc='center'))
    d.add(elm.Label().at(Point((9, TITLE_Y + 0.2)))
          .label('ESP32-S3 N16R8 · 64×32 HUB75 · MAX98357A · 3 buttons', loc='center')
          .color('#888888'))

    # ═══════════════════════════════════════════════════════════════════════
    # SECTION 1 — POWER DISTRIBUTION
    # ═══════════════════════════════════════════════════════════════════════
    section_title('Power Distribution  (5V @ 4A)', 9, SEC_POWER_Y + 2.5)

    py = SEC_POWER_Y          # +5V rail y
    gy = SEC_POWER_Y - 1.2   # GND rail y
    x0 = 1.0

    # Barrel jack
    jack = d.add(elm.Ic(
        pins=[
            elm.IcPin(name='+',   side='right', slot='1/2', anchorname='pos'),
            elm.IcPin(name='GND', side='right', slot='2/2', anchorname='neg'),
        ],
        edgepadH=0.25, edgepadW=0.9,
    ).label('5V 4A\nbarrel jack', loc='center', fontsize=8).at(Point((x0, py))))

    rail_x0 = jack.absanchors['pos'].x
    rail_y  = jack.absanchors['pos'].y
    gnd_y   = jack.absanchors['neg'].y

    # Rails
    d.add(elm.Line().right(15).at(jack.absanchors['pos']))
    d.add(elm.Line().right(15).at(jack.absanchors['neg']))

    def tap(x, top_lbl, bot_lbl=None, cap_val=None):
        # Tap from +5V rail
        d.add(elm.Dot().at(Point((x, rail_y))))
        d.add(elm.Line().up(0.8).at(Point((x, rail_y))).label(top_lbl, 'top'))
        # GND tap
        d.add(elm.Dot().at(Point((x, gnd_y))))
        if bot_lbl:
            d.add(elm.Line().down(0.7).at(Point((x, gnd_y))).label(bot_lbl, 'bottom'))
        else:
            d.add(elm.Ground().at(Point((x, gnd_y))))
        # Optional decoupling cap
        if cap_val:
            cx = x + 0.7
            d.add(elm.Dot().at(Point((cx, rail_y))))
            d.add(elm.Capacitor()
                  .down(rail_y - gnd_y)
                  .at(Point((cx, rail_y)))
                  .label(cap_val, 'right'))
            d.add(elm.Dot().at(Point((cx, gnd_y))))

    tap(rail_x0 + 1.2, '470 µF',  cap_val=None)   # bulk cap (drawn as tap)
    d.add(elm.Capacitor()
          .down(rail_y - gnd_y)
          .at(Point((rail_x0 + 1.2, rail_y)))
          .label('470 µF', 'right'))
    d.add(elm.Dot().at(Point((rail_x0 + 1.2, gnd_y))))

    tap(rail_x0 + 3.0, 'LED panel\n5V screw term.', cap_val='100 nF')
    tap(rail_x0 + 6.5, 'ESP32-S3\nVIN → LDO 3.3V', cap_val='100 nF')
    tap(rail_x0 + 10.5, 'MAX98357A\nVIN', cap_val='100 nF')

    # ═══════════════════════════════════════════════════════════════════════
    # SECTION 2 — HUB75 INTERFACE
    # ═══════════════════════════════════════════════════════════════════════
    section_title('HUB75 Interface  (13 signals, 16-pin IDC)', 7, SEC_HUB75_Y + N * 0.58 + 1.0)

    esp_hub = d.add(elm.Ic(
        pins=[
            elm.IcPin(name=sig, pin=f'GPIO {gpio}', side='right',
                      slot=f'{i+1}/{N}', anchorname=f'e_{sig}')
            for i, (sig, gpio) in enumerate(HUB75_PINS)
        ] + [
            elm.IcPin(name='GND',   side='left', slot='1/2', anchorname='e_gnd'),
            elm.IcPin(name='+3.3V', side='left', slot='2/2', anchorname='e_3v3'),
        ],
        edgepadH=0.25, edgepadW=1.5,
    ).label('ESP32-S3', loc='center', fontsize=10)
     .at(Point((0, SEC_HUB75_Y))))

    hub_con = d.add(elm.Ic(
        pins=[
            elm.IcPin(name=sig, pin=str(i + 1), side='left',
                      slot=f'{i+1}/{N}', anchorname=f'c_{sig}')
            for i, (sig, _) in enumerate(HUB75_PINS)
        ] + [
            elm.IcPin(name='GND(14,16)', side='right', slot='1/1', anchorname='c_gnd'),
        ],
        edgepadH=0.25, edgepadW=0.9,
    ).label('HUB75\n16-pin IDC', loc='center', fontsize=9)
     .at(Point((esp_hub.absanchors['e_R1'].x + 4.5,
                esp_hub.absanchors['e_R1'].y))))

    for sig, _ in HUB75_PINS:
        d.add(elm.Line().at(esp_hub.absanchors[f'e_{sig}'])
                        .to(hub_con.absanchors[f'c_{sig}']))

    d.add(elm.Ground().at(esp_hub.absanchors['e_gnd']))
    d.add(elm.Line().left(0.6).at(esp_hub.absanchors['e_3v3'])
          .label('+3.3V', 'left'))
    d.add(elm.Ground().at(hub_con.absanchors['c_gnd']))

    # ═══════════════════════════════════════════════════════════════════════
    # SECTION 3 — AUDIO INTERFACE
    # ═══════════════════════════════════════════════════════════════════════
    section_title('Audio Interface  (I2S → MAX98357A → speaker)', 4, SEC_AUDIO_Y + 5.8)

    esp_aud = d.add(elm.Ic(
        pins=[
            elm.IcPin(name='BCLK', pin='GPIO 9',  side='right', slot='1/3', anchorname='bclk'),
            elm.IcPin(name='WS',   pin='GPIO 10', side='right', slot='2/3', anchorname='ws'),
            elm.IcPin(name='DATA', pin='GPIO 11', side='right', slot='3/3', anchorname='data'),
            elm.IcPin(name='GND',  side='left', slot='1/2', anchorname='gnd'),
            elm.IcPin(name='+3.3V',side='left', slot='2/2', anchorname='pwr'),
        ],
        edgepadH=0.5, edgepadW=1.5,
    ).label('ESP32-S3\n(I2S)', loc='center', fontsize=9)
     .at(Point((AUDIO_X, SEC_AUDIO_Y))))

    bclk = esp_aud.absanchors['bclk']

    amp = d.add(elm.Ic(
        pins=[
            elm.IcPin(name='BCLK', side='left',  slot='1/5', anchorname='bclk'),
            elm.IcPin(name='WS',   side='left',  slot='2/5', anchorname='ws'),
            elm.IcPin(name='DIN',  side='left',  slot='3/5', anchorname='din'),
            elm.IcPin(name='VIN',  side='left',  slot='4/5', anchorname='vin'),
            elm.IcPin(name='GND',  side='left',  slot='5/5', anchorname='gnd'),
            elm.IcPin(name='OUT+', side='right', slot='1/2', anchorname='outp'),
            elm.IcPin(name='OUT-', side='right', slot='2/2', anchorname='outn'),
        ],
        edgepadH=0.5, edgepadW=1.0,
    ).label('MAX98357A\n3W Class D', loc='center', fontsize=9)
     .at(Point((bclk.x + 4.0, bclk.y))))

    d.add(elm.Line().at(esp_aud.absanchors['bclk']).to(amp.absanchors['bclk']))
    d.add(elm.Line().at(esp_aud.absanchors['ws']).to(amp.absanchors['ws']))
    d.add(elm.Line().at(esp_aud.absanchors['data']).to(amp.absanchors['din']))

    # +5V power to amp — route over the top
    vin  = amp.absanchors['vin']
    top_y = bclk.y + 2.5
    d.add(elm.Line().up(top_y - vin.y).at(vin))
    d.add(elm.Line().left(vin.x - (AUDIO_X - 0.5)).at(Point((vin.x, top_y))))
    d.add(elm.Line().up(0.5).at(Point((AUDIO_X - 0.5, top_y))).label('+5V', 'top'))

    d.add(elm.Ground().at(esp_aud.absanchors['gnd']))
    d.add(elm.Ground().at(amp.absanchors['gnd']))
    d.add(elm.Line().left(0.5).at(esp_aud.absanchors['pwr']).label('+3.3V', 'left'))

    # Speaker
    outp = amp.absanchors['outp']
    outn = amp.absanchors['outn']
    spk = d.add(elm.Ic(
        pins=[
            elm.IcPin(name='+', side='left', slot='1/2', anchorname='p'),
            elm.IcPin(name='-', side='left', slot='2/2', anchorname='n'),
        ],
        edgepadH=0.3, edgepadW=0.5,
    ).label('SPK\n4Ω 3W', loc='center', fontsize=9)
     .at(Point((outp.x + 2.5, outp.y))))

    d.add(elm.Line().at(outp).to(spk.absanchors['p']))
    d.add(elm.Line().at(outn).to(spk.absanchors['n']))

    # ═══════════════════════════════════════════════════════════════════════
    # SECTION 4 — BUTTONS
    # ═══════════════════════════════════════════════════════════════════════
    section_title('User Interface  (3 × tactile + 10 kΩ pull-down)', BUTTONS_X + 5, SEC_BUTTONS_Y + 5.8)

    BTNS = [
        ('GPIO 33', 'Next scene'),
        ('GPIO 34', 'Brightness'),
        ('GPIO 35', 'WiFi setup\n(hold 5s)'),
    ]
    for i, (gpio, lbl) in enumerate(BTNS):
        bx = BUTTONS_X + i * 3.8
        by = SEC_BUTTONS_Y + 3.5

        d.add(elm.Line().up(0.5).at(Point((bx, by))).label('+3.3V', 'top'))
        d.add(elm.Button().down(1.5).at(Point((bx, by))).label(lbl, 'right'))
        jct = Point((bx, by - 1.5))
        d.add(elm.Dot().at(jct))
        d.add(elm.Line().right(1.4).at(jct).label(f'→ {gpio}', 'right'))
        d.add(elm.Resistor().down(1.8).at(jct).label('10 kΩ', 'right'))
        d.add(elm.Ground())

    # ── save ─────────────────────────────────────────────────────────────────
    d.save(str(OUT / 'schematic.svg'))
    print('✓ schematic.svg')

# Clean up individual files if they exist from previous runs
for old in ['sch_power.svg', 'sch_hub75.svg', 'sch_audio.svg', 'sch_buttons.svg']:
    p = OUT / old
    if p.exists():
        p.unlink()
        print(f'  removed {old}')
