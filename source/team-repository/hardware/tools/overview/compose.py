#!/usr/bin/env python3
"""Compose one self-contained SVG from validated native KiCad sheet exports.

The detailed circuits are embedded as vectors, not redrawn or rasterized.
The connection map is checked against the exported electrical netlist.
"""
from pathlib import Path
import copy
import json
import math
import xml.etree.ElementTree as ET

SVG = 'http://www.w3.org/2000/svg'
XLINK = 'http://www.w3.org/1999/xlink'
ET.register_namespace('', SVG)
ET.register_namespace('xlink', XLINK)
ROOT_NAME = 'panic_at_the_pan_interfaces'
SHEETS = [
    ('power_pic32', '01 Power + PIC32 core', '01  POWER + PIC32', 'Core supply, ICSP, UART and all allocated PIC pins'),
    ('inputs', '03 Inputs', '03  INPUTS + EXPANDER', 'Dedicated SPI2, switches, ADC, ultrasonic and Finish LED'),
    ('displays', '02 Displays', '02  DISPLAYS', 'Dedicated SPI1, level shifting and centered 3-4-3 pan'),
    ('actuators', '04 Actuators', '04  ACTUATORS', 'Proposed STEP/DIR driver, home switch and treat servo'),
    ('audio', '05 Audio', '05  AUDIO', 'PWM filter, proposed amplifier, speaker and mute'),
]

# These endpoints also define the labels on the connection map.
SIGNALS = {
    'DISPLAY_MOSI': ('U1.3', 'U2.4'),
    'DISPLAY_SCK': ('U1.25', 'U2.2'),
    'DISPLAY_SS': ('U1.2', 'U2.6'),
    'EXPANDER_MOSI': ('U1.22', 'U11.13'),
    'EXPANDER_MISO': ('U1.24', 'U11.14'),
    'EXPANDER_SCK': ('U1.26', 'U11.12'),
    'EXPANDER_SS': ('U1.17', 'U11.11'),
    'JOY_X_ADC': ('U1.6', 'J21.3'),
    'JOY_Y_ADC': ('U1.7', 'J21.4'),
    'US_TRIG': ('U1.23', 'U10.2'),
    'US_ECHO_3V3': ('U1.9', 'J23.2'),
    'SPATULA_HOME': ('U1.21', 'SW40.1'),
    'SPATULA_STEP': ('U1.12', 'U20.STEP'),
    'SPATULA_DIR': ('U1.11', 'U20.DIR'),
    'SPATULA_WAKE': ('U1.18', 'U20.nSLEEP'),
    'TREAT_PWM': ('U1.14', 'U21.2'),
    'AUDIO_PWM': ('U1.10', 'R60.1'),
}


def element(parent, tag, attrs=None, text=None):
    item = ET.SubElement(parent, f'{{{SVG}}}{tag}', {k: str(v) for k, v in (attrs or {}).items()})
    if text is not None:
        item.text = text
    return item


def rect(parent, x, y, w, h, fill='white', stroke='#ccd3db', radius=8):
    return element(parent, 'rect', dict(x=x, y=y, width=w, height=h, rx=radius, fill=fill, stroke=stroke, **{'stroke-width': 1.5}))


def text(parent, x, y, value, size=18, color='#233246', weight=400, anchor='start'):
    return element(parent, 'text', {'x': x, 'y': y, 'fill': color, 'font-family': 'Arial, Helvetica, sans-serif', 'font-size': size, 'font-weight': weight, 'text-anchor': anchor}, value)


def lines(parent, x, y, values, size=17, color='#4a596b', gap=25):
    for i, value in enumerate(values):
        text(parent, x, y + i * gap, value, size, color)


def path(parent, points, color, arrow=True, dashed=False):
    attrs = {'d': 'M ' + ' L '.join(f'{x} {y}' for x, y in points), 'fill': 'none', 'stroke': color, 'stroke-width': 3}
    if dashed:
        attrs['stroke-dasharray'] = '7 5'
    result = element(parent, 'path', attrs)
    if arrow:
        # Explicit geometry also works in viewers that omit SVG marker elements.
        x, y = points[-1]; px, py = points[-2]
        length = math.hypot(x-px, y-py)
        ux, uy = (x-px)/length, (y-py)/length
        bx, by = x-11*ux, y-11*uy
        element(parent, 'polygon', {'points': f'{x},{y} {bx-5*uy},{by+5*ux} {bx+5*uy},{by-5*ux}', 'fill': color})
    return result


def checked_netlist(path):
    tree = ET.parse(path)
    nets = {}
    for net in tree.findall('./nets/net'):
        for pin in net.findall('node'):
            nets[f'{pin.attrib["ref"]}.{pin.attrib["pin"]}'] = net.attrib['name']
    for name, endpoints in SIGNALS.items():
        if any(nets.get(pin) != name for pin in endpoints):
            raise ValueError(f'Overview map disagrees with schematic: {name}: {endpoints}')
    # Supporting connections shown in the map must remain actual electrical nets.
    for endpoints in [
        ['U11.1', 'U2.8', 'R36.1'], ['U2.12', 'R27.1'], ['R27.2', 'Q10.2'],
        ['U11.27', 'J24.1'], ['U11.9', 'U1.13', 'U1.28'],
        ['J4.1', 'U2.20', 'U30.VIN'], ['J7.1', 'U20.VMOT'],
        ['U1.8', 'U11.10', 'Q10.1'],
    ]:
        values = [nets.get(pin) for pin in endpoints]
        if None in values or len(set(values)) != 1:
            raise ValueError(f'Overview support connection mismatch: {endpoints}')
    for i in range(6):
        if nets.get(f'U11.{21+i}') != nets.get(f'SW{20+i}.2'):
            raise ValueError(f'Overview switch mapping mismatch: GPA{i}')
    return nets


def block(parent, x, y, w, h, heading, body, color='#233246', fill='#f7f9fc'):
    rect(parent, x, y, w, h, fill, color)
    text(parent, x + 18, y + 30, heading, 20, color, 700)
    lines(parent, x + 18, y + 60, body, 16, gap=23)


def connection_map(parent):
    """Readable functional routing; detailed pin circuitry is in the five panels."""
    blue, teal, grey = '#245ec4', '#007f79', '#53627a'
    block(parent, 25, 20, 1150, 90, 'POWER DISTRIBUTION', [
        '12 V / 5 A: breadboard regulator -> 3.3 V logic (~100 mA allowance); separate 12 V motor branch.',
        '5 V / 3 A: shared display / buffer / sensor / servo / audio rail. All modules share GNDREF.',
    ], color='#9a4a22', fill='#fff8f1')

    block(parent, 390, 150, 310, 645, 'U1  PIC32MX170F256B', [
        '28-pin DIP / 3.3 V',
        '17 / 17 application GPIO allocated',
    ], fill='#edf2f8')
    text(parent, 414, 285, 'SPI1  ->  displays', 20, blue, 700)
    text(parent, 414, 420, 'SPI2  <->  expander', 20, teal, 700)
    lines(parent, 414, 485, ['Analog inputs stay on PIC.', 'Timed signals stay on PIC.', 'Game logic coordinates both buses.'], 16)
    lines(parent, 414, 635, ['SNAP: pins 1 / 4 / 5', 'UART: TX 16 / RX 15', 'VDD 13; AVDD 28', 'GND 8 / 19 / 27; VCAP 20'], 16)
    text(parent, 414, 770, 'Signal pin numbers refer to PIC pins.', 15, '#4a596b')

    block(parent, 25, 175, 265, 105, 'JOYSTICK', ['X: pin 6 / AN4', 'Y: pin 7 / AN5; 0-3.3 V'])
    path(parent, [(290, 230), (390, 230)], grey)
    text(parent, 340, 216, 'ADC', 16, grey, anchor='middle')

    block(parent, 25, 330, 265, 150, 'ULTRASONIC', ['TRIG: pin 23 -> U10 buffer', 'ECHO: divider -> pin 9', 'Sensor model remains TBD.'])
    path(parent, [(390, 375), (290, 375)], grey)
    path(parent, [(290, 440), (390, 440)], grey)
    text(parent, 340, 360, 'TRIG', 15, grey, anchor='middle')
    text(parent, 340, 426, 'ECHO', 15, grey, anchor='middle')

    block(parent, 25, 540, 265, 95, 'SPATULA HOME', ['Reference switch -> pin 21'])
    path(parent, [(290, 588), (390, 588)], grey)

    block(parent, 25, 690, 265, 105, 'FLIP RESERVED', ['MCP GPA6 / pin 27', 'Active-low contact; optional.'], teal, '#eef9f7')

    block(parent, 820, 150, 355, 135, 'SPI1 / DISPLAYS', ['MOSI 3; SCK 25; SS 2', '74ACT244 -> 10 pan modules', 'Centered 3-4-3 + score/timer/heat'], blue, '#eff4ff')
    path(parent, [(700, 235), (820, 235)], blue)
    text(parent, 760, 219, 'SPI1', 18, blue, 700, 'middle')

    block(parent, 820, 320, 355, 235, 'SPI2 / MCP23S17', ['MOSI 22; MISO 24; SCK 26; SS 17', 'GPA0-4: five ingredients', 'GPA5: Finish switch', 'GPA6: reserved flip input', 'GPB0 -> U2 8->12 -> 2N7000', '                         -> Finish LED'], teal, '#eef9f7')
    path(parent, [(700, 395), (820, 395)], teal)
    path(parent, [(820, 425), (700, 425)], teal)
    text(parent, 760, 380, 'SPI2', 18, teal, 700, 'middle')

    block(parent, 820, 590, 355, 110, 'ACTUATORS', ['STEP 12 / DIR 11 / WAKE 18', 'Treat PWM: pin 14 / OC2'], grey)
    path(parent, [(700, 635), (820, 635)], grey)

    block(parent, 820, 730, 355, 65, 'AUDIO PWM: PIN 10 / OC3', [], grey)
    path(parent, [(700, 762), (820, 762)], grey)
    path(parent, [(155, 795), (155, 818), (1190, 818), (1190, 449), (1175, 449)], teal, dashed=True)
    # The detailed circuit panels below resolve every local connector/component.


def embedded_sheet(parent, source, x, y, width, height, prefix):
    node = copy.deepcopy(ET.parse(source).getroot())
    ids = {item.attrib['id']: f'{prefix}-{item.attrib["id"]}' for item in node.iter() if 'id' in item.attrib}
    for item in node.iter():
        for key, value in list(item.attrib.items()):
            if key == 'id':
                item.set(key, ids[value])
            else:
                for old, new in ids.items():
                    value = value.replace(f'url(#{old})', f'url(#{new})')
                    if value == f'#{old}':
                        value = f'#{new}'
                item.set(key, value)
    min_x, min_y, view_width, view_height = map(float, node.attrib['viewBox'].split())
    scale = min(width / view_width, height / view_height)
    offset_x = x + (width - view_width * scale) / 2
    offset_y = y + (height - view_height * scale) / 2
    # Plain transformed groups render consistently in browsers and SVG/PDF viewers.
    group = element(parent, 'g', {'id': f'sheet-{prefix}', 'transform': f'translate({offset_x} {offset_y}) scale({scale}) translate({-min_x} {-min_y})'})
    for child in node:
        group.append(child)


def compose(export_dir, netlist, destination, check_count):
    export_dir, destination = Path(export_dir), Path(destination)
    nets = checked_netlist(netlist)
    root = ET.Element(f'{{{SVG}}}svg', {'version': '1.1', 'width': '3720', 'height': '2135', 'viewBox': '0 0 3720 2135', 'role': 'img', 'aria-labelledby': 'overview-title overview-desc'})
    element(root, 'title', {'id': 'overview-title'}, 'Panic at the Pan — complete system schematic, pin map V2')
    element(root, 'desc', {'id': 'overview-desc'}, 'One self-contained vector drawing: a system connection map and the five complete KiCad circuit sheets. SPI1 and SPI2 are electrically separate. Repeated signal and power labels join the circuit panels. Proposed components and unverified power capacity remain marked.')
    element(root, 'metadata', text=json.dumps({'pin_map': 'V2', 'source': 'native KiCad hierarchy and checked netlist', 'module_count': len(SHEETS), 'application_signals': {name: {'net': nets[pins[0]], 'pins': pins} for name, pins in SIGNALS.items()}}))
    rect(root, 0, 0, 3720, 2135, '#edf1f5', 'none', 0)
    text(root, 40, 63, 'PANIC AT THE PAN', 39, '#13263c', 700)
    text(root, 40, 101, 'COMPLETE SYSTEM SCHEMATIC  /  PIN MAP V2  /  09 NOV 2026', 22, '#435975')
    text(root, 3680, 58, 'SPI1: DISPLAYS    |    SPI2: EXPANDER', 23, '#13263c', 700, 'end')
    text(root, 3680, 94, f'{check_count} connection checks + ERC • schematic review; no bench validation', 18, '#52627a', anchor='end')

    positions = [(30, 135), (1260, 135), (2490, 135), (30, 1085), (1260, 1085), (2490, 1085)]
    for index, (x, y) in enumerate(positions):
        rect(root, x, y, 1200, 920, '#ffffff', '#bcc8d5', 12)
        if index == 0:
            heading, subtitle = '00  SYSTEM CONNECTION MAP', 'Functional routing; complete component circuits are in panels 01-05.'
        else:
            module, sheet_name, heading, subtitle = SHEETS[index-1]
        text(root, x+22, y+33, heading, 23, '#13263c', 700)
        text(root, x+22, y+60, subtitle, 16, '#52627a')
        if index == 0:
            group = element(root, 'g', {'transform': f'translate({x} {y+83})'})
            connection_map(group)
        else:
            source = export_dir / f'{ROOT_NAME}-{sheet_name}.svg'
            embedded_sheet(root, source, x+8, y+76, 1184, 837, module)
    text(root, 40, 2044, 'READING THE DRAWING', 19, '#13263c', 700)
    text(root, 40, 2072, 'Identical global net labels connect across panels; identical local labels connect within a panel. Zoom in for component values and physical pin numbers.', 19, '#435975')
    text(root, 40, 2102, 'STILL TBD: motor/servo/sensor/audio parts, Finish lamp + R29, connector details and measured power budget. Firmware initialization is not yet implemented.', 19, '#83502b')
    # Reject external images: the deliverable must remain portable and vector-only.
    if any(item.tag == f'{{{SVG}}}image' for item in root.iter()):
        raise ValueError('Overview contains a raster or external image')
    svg = '<?xml version="1.0" encoding="UTF-8"?>\n' + ET.tostring(root, encoding='unicode')
    destination.write_text('\n'.join(line.rstrip() for line in svg.splitlines()) + '\n')
