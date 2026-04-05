"""
orrery — documentation schematic generator (schemdraw 0.22)
Outputs:
  docs/sch_power.svg   — 5V power distribution
  docs/sch_hub75.svg   — ESP32 → HUB75 connector interface
  docs/sch_audio.svg   — ESP32 I2S → MAX98357A → speaker
  docs/sch_buttons.svg — tactile button circuits with pull-downs
"""

import schemdraw
import schemdraw.elements as elm
from schemdraw.util import Point
from pathlib import Path
import matplotlib
matplotlib.use("Agg")

OUT = Path(__file__).parent

# Valid label locs in schemdraw 0.22: 'top','bottom','left','right','center'


# ─── 1. Power distribution ────────────────────────────────────────────────────

with schemdraw.Drawing(show=False) as d:
    d.config(fontsize=8.5, inches_per_unit=0.55)

    # Barrel jack
    jack = d.add(elm.Ic(
        pins=[
            elm.IcPin(name='+',   side='right', slot='1/2', anchorname='pos'),
            elm.IcPin(name='GND', side='right', slot='2/2', anchorname='neg'),
        ],
        edgepadH=0.3, edgepadW=0.9,
    ).label('5.5/2.1mm\nbarrel jack', loc='center', fontsize=8))

    rail_y = jack.absanchors['pos'].y
    gnd_y  = jack.absanchors['neg'].y
    x0     = jack.absanchors['pos'].x

    # +5V horizontal rail
    d.add(elm.Line().right(8.0).at(jack.absanchors['pos']))
    d.add(elm.Line().right(8.0).at(jack.absanchors['neg']))

    # Helper: tap up from +5V rail with label
    def tap_up(d, x, lbl, rail_y=rail_y):
        d.add(elm.Dot().at(Point((x, rail_y))))
        d.add(elm.Line().up(0.8).at(Point((x, rail_y))).label(lbl, 'top'))

    def tap_down(d, x, lbl, gnd_y=gnd_y):
        d.add(elm.Dot().at(Point((x, gnd_y))))
        d.add(elm.Line().down(0.8).at(Point((x, gnd_y))).label(lbl, 'bottom'))

    def decoupling(d, x, val='100nF'):
        d.add(elm.Dot().at(Point((x, rail_y))))
        d.add(elm.Capacitor().down(rail_y - gnd_y).at(Point((x, rail_y))).label(val, 'right'))
        d.add(elm.Dot().at(Point((x, gnd_y))))

    # Bulk cap right after jack
    decoupling(d, x0 + 1.2, '470µF')

    # LED panel
    tap_up(d,   x0 + 2.5, 'LED panel\n(5V, screw term.)')
    tap_down(d, x0 + 2.5, 'GND')
    decoupling(d, x0 + 3.3, '100nF')

    # ESP32
    tap_up(d,   x0 + 5.0, 'ESP32-S3 VIN\n→ 3.3V LDO')
    tap_down(d, x0 + 5.0, 'GND')
    decoupling(d, x0 + 5.8, '100nF')

    # MAX98357A
    tap_up(d,   x0 + 7.2, 'MAX98357A\nVIN')
    tap_down(d, x0 + 7.2, 'GND')

    d.save(str(OUT / 'sch_power.svg'))
    print('✓ sch_power.svg')


# ─── 2. HUB75 interface ───────────────────────────────────────────────────────

HUB75_PINS = [
    ('R1', '4'), ('G1', '5'), ('B1', '6'),
    ('R2', '7'), ('G2', '15'), ('B2', '16'),
    ('A', '36'), ('B', '37'), ('C', '38'), ('D', '39'),
    ('CLK', '2'), ('LAT', '47'), ('OE', '48'),
]

with schemdraw.Drawing(show=False) as d:
    d.config(fontsize=7.5, inches_per_unit=0.48)

    n = len(HUB75_PINS)

    esp = d.add(elm.Ic(
        pins=[
            elm.IcPin(name=sig, pin=f'GPIO {gpio}', side='right',
                      slot=f'{i+1}/{n}', anchorname=f'e_{sig}')
            for i, (sig, gpio) in enumerate(HUB75_PINS)
        ] + [
            elm.IcPin(name='GND',   pin='', side='left', slot='1/2', anchorname='e_gnd'),
            elm.IcPin(name='+3.3V', pin='', side='left', slot='2/2', anchorname='e_pwr'),
        ],
        edgepadH=0.25, edgepadW=1.4,
    ).label('ESP32-S3', loc='center', fontsize=10))

    con = d.add(elm.Ic(
        pins=[
            elm.IcPin(name=sig, pin=str(i + 1), side='left',
                      slot=f'{i+1}/{n}', anchorname=f'c_{sig}')
            for i, (sig, _) in enumerate(HUB75_PINS)
        ] + [
            elm.IcPin(name='GND', pin='14,16', side='right', slot='1/1', anchorname='c_gnd'),
        ],
        edgepadH=0.25, edgepadW=0.8,
    ).label('HUB75\n16-pin IDC', loc='center', fontsize=9)
     .at(Point((esp.absanchors['e_R1'].x + 4.0, esp.absanchors['e_R1'].y))))

    # Signal wires
    for sig, _ in HUB75_PINS:
        d.add(elm.Line().at(esp.absanchors[f'e_{sig}']).to(con.absanchors[f'c_{sig}']))

    # GND + power on ESP32 left side
    d.add(elm.Ground().at(esp.absanchors['e_gnd']))
    d.add(elm.Line().left(0.6).at(esp.absanchors['e_pwr']).label('+3.3V', 'left'))

    # GND on connector right side
    d.add(elm.Ground().at(con.absanchors['c_gnd']))

    d.save(str(OUT / 'sch_hub75.svg'))
    print('✓ sch_hub75.svg')


# ─── 3. Audio interface ───────────────────────────────────────────────────────

with schemdraw.Drawing(show=False) as d:
    d.config(fontsize=8.5, inches_per_unit=0.55)

    esp = d.add(elm.Ic(
        pins=[
            elm.IcPin(name='BCLK', pin='GPIO 9',  side='right', slot='1/3', anchorname='bclk'),
            elm.IcPin(name='WS',   pin='GPIO 10', side='right', slot='2/3', anchorname='ws'),
            elm.IcPin(name='DATA', pin='GPIO 11', side='right', slot='3/3', anchorname='data'),
            elm.IcPin(name='GND',  pin='',        side='left',  slot='1/2', anchorname='gnd'),
            elm.IcPin(name='+3.3V',pin='',        side='left',  slot='2/2', anchorname='pwr'),
        ],
        edgepadH=0.5, edgepadW=1.4,
    ).label('ESP32-S3\n(I2S)', loc='center', fontsize=9))

    bclk = esp.absanchors['bclk']

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
    ).label('MAX98357A\nClass D 3W', loc='center', fontsize=9)
     .at(Point((bclk.x + 4.0, bclk.y))))

    # I2S wires
    d.add(elm.Line().at(esp.absanchors['bclk']).to(amp.absanchors['bclk']))
    d.add(elm.Line().at(esp.absanchors['ws']).to(amp.absanchors['ws']))
    d.add(elm.Line().at(esp.absanchors['data']).to(amp.absanchors['din']))

    # Power to amp — route up then across then down to VIN
    vin  = amp.absanchors['vin']
    top_y = bclk.y + 2.0
    d.add(elm.Line().up(top_y - vin.y).at(vin))
    d.add(elm.Line().left(vin.x - bclk.x + 0.5).at(Point((vin.x, top_y))))
    d.add(elm.Line().up(0.5).at(Point((bclk.x - 0.5, top_y))).label('+5V', 'top'))

    # GND on both ICs
    d.add(elm.Ground().at(esp.absanchors['gnd']))
    d.add(elm.Ground().at(amp.absanchors['gnd']))

    # +3.3V label on ESP32 left
    d.add(elm.Line().left(0.5).at(esp.absanchors['pwr']).label('+3.3V', 'left'))

    # Speaker — right of amp
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

    d.save(str(OUT / 'sch_audio.svg'))
    print('✓ sch_audio.svg')


# ─── 4. Button circuits ───────────────────────────────────────────────────────

BUTTONS = [
    ('GPIO 33', 'Next scene'),
    ('GPIO 34', 'Brightness'),
    ('GPIO 35', 'WiFi setup\n(hold 5s)'),
]

with schemdraw.Drawing(show=False) as d:
    d.config(fontsize=8.5, inches_per_unit=0.55)

    col_spacing = 3.5

    for i, (gpio, label) in enumerate(BUTTONS):
        x = i * col_spacing

        # +3.3V at top
        d.add(elm.Line().up(0.5).at(Point((x, 3.5))).label('+3.3V', 'top'))

        # Button
        d.add(elm.Button().down(1.5).at(Point((x, 3.5))).label(label, 'right'))

        # Junction
        jct = Point((x, 2.0))
        d.add(elm.Dot().at(jct))

        # GPIO label wire right
        d.add(elm.Line().right(1.5).at(jct).label(f'→ {gpio}', 'right'))

        # Pull-down resistor
        d.add(elm.Resistor().down(1.8).at(jct).label('10 kΩ', 'right'))
        d.add(elm.Ground())

    d.save(str(OUT / 'sch_buttons.svg'))
    print('✓ sch_buttons.svg')
