#!/usr/bin/env python3
"""Export and check the editable KiCad hierarchy; never regenerate schematics."""
from pathlib import Path
import argparse, json, os, runpy, shutil, subprocess, tempfile, xml.etree.ElementTree as ET

HARDWARE = Path(__file__).resolve().parents[2]
TOOLS = HARDWARE / 'tools'
MODULES = ['overview', 'power_pic32', 'displays', 'inputs', 'actuators', 'audio']
ROOT_NAME = 'panic_at_the_pan_interfaces'
SHEET_NAMES = ['','01 Power + PIC32 core','02 Displays','03 Inputs','04 Actuators','05 Audio']


def check_connections(netlist):
    tree = ET.parse(netlist)
    nets = {}
    for net in tree.findall('./nets/net'):
        for node in net.findall('node'):
            nets[f'{node.attrib["ref"]}.{node.attrib["pin"]}'] = net.attrib['code']
    results = []
    for module in MODULES:
        spec = json.loads((TOOLS / module / 'checks.json').read_text())
        count = 0
        for rule in spec.get('same_net', []):
            missing = [pin for pin in rule if pin not in nets]
            if missing or len({nets.get(pin) for pin in rule}) != 1:
                raise RuntimeError(f'{module}: expected connected {rule}; missing {missing}')
            count += 1
        for rule in spec.get('different_nets', []):
            if any(pin not in nets for pin in rule) or len({nets.get(pin) for pin in rule}) != len(rule):
                raise RuntimeError(f'{module}: expected separate nets {rule}')
            count += 1
        results.append({'module': module, 'connection_checks': count, 'status': 'passed'})
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kicad-cli', default=os.environ.get('KICAD_CLI'))
    args = parser.parse_args()
    cli = args.kicad_cli or shutil.which('kicad-cli')
    if not cli:
        candidate = '/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli'
        if Path(candidate).exists(): cli = candidate
    if not cli: parser.error('Install KiCad or set KICAD_CLI.')
    source = HARDWARE / (ROOT_NAME + '.kicad_sch')
    review = HARDWARE / 'review'
    with tempfile.TemporaryDirectory(prefix='pan-schematic-review-') as tmp:
        out = Path(tmp)
        commands = [
            ['sch','erc','--format','json','--exit-code-violations','-o',str(out/'erc.json')],
            ['sch','export','netlist','--format','kicadxml','-o',str(out/'interfaces.net.xml')],
            ['sch','export','svg','-o',str(out)+os.sep],
        ]
        for command in commands:
            subprocess.run([cli,*command,str(source)],check=True,cwd=HARDWARE)
        erc = json.loads((out/'erc.json').read_text())
        if len(erc['sheets']) != 6 or any(s['violations'] for s in erc['sheets']):
            raise RuntimeError('Expected six sheets and zero ERC violations.')
        results = check_connections(out/'interfaces.net.xml')
        compose = runpy.run_path(str(TOOLS/'overview'/'compose.py'))['compose']
        compose(out, out/'interfaces.net.xml', out/'complete-system.svg',
                sum(r['connection_checks'] for r in results))
        # Publish artifacts only after every check succeeds.
        for module,sheet_name in zip(MODULES,SHEET_NAMES):
            folder = review/module
            folder.mkdir(parents=True,exist_ok=True)
            filename=ROOT_NAME + ('-'+sheet_name if sheet_name else '') + '.svg'
            svg=(out/filename).read_text()
            target = 'hierarchy.svg' if module == 'overview' else 'schematic.svg'
            (folder/target).write_text('\n'.join(line.rstrip() for line in svg.splitlines())+'\n')
        shutil.copyfile(out/'complete-system.svg', review/'overview'/'schematic.svg')
        system=review/'system';system.mkdir(parents=True,exist_ok=True)
        for name in ['erc.json','interfaces.net.xml']:
            shutil.copyfile(out/name,system/name)
        (system/'connections.json').write_text(json.dumps(results,indent=2)+'\n')
    print(f'Passed ERC and {sum(r["connection_checks"] for r in results)} connection checks across six sheets.')

if __name__ == '__main__': main()
