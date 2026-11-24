# Schematic tools

Run from the repository root:

```sh
python3 hardware/tools/shared/review.py
```

Requires Python 3 and KiCad CLI. Set `KICAD_CLI` if it is not found automatically.

- `overview/`, `power_pic32/`, `displays/`, `inputs/`, `actuators/`, `audio/`: connection-check definitions.
- `shared/review.py`: checks the whole hierarchy and exports previews into matching `hardware/review/` folders.
- `overview/compose.py`: checks the system connection map against the netlist and combines all five native circuit exports into `review/overview/schematic.svg`. The original hierarchy preview is retained as `review/overview/hierarchy.svg`.

Native KiCad files are the source of truth. This tool never regenerates or overwrites schematics. The obsolete P0 generator was retired.
