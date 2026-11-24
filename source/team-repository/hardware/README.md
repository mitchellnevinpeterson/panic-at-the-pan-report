# Hardware

[Open KiCad project](panic_at_the_pan_interfaces.kicad_pro). Double-click a module box in the overview, or use the Hierarchy Navigator.

[Complete system schematic](review/overview/schematic.svg): one zoomable drawing with all five circuits and their system connections.

| Module | Schematic | Notes |
|---|---|---|
| Power + PIC32 | [Open](sheets/power_pic32.kicad_sch) | [Read](docs/power_pic32.md) |
| Displays | [Open](sheets/displays.kicad_sch) | [Read](docs/displays.md) |
| Inputs | [Open](sheets/inputs.kicad_sch) | [Read](docs/inputs.md) |
| Actuators | [Open](sheets/actuators.kicad_sch) | [Read](docs/actuators.md) |
| Audio | [Open](sheets/audio.kicad_sch) | [Read](docs/audio.md) |

- `sheets/`: module schematics.
- `symbols/`: component symbol libraries; registered in `sym-lib-table`.
- `docs/`: module explanations and integration decisions.
- `review/`: previews and check results, grouped by module.
- `tools/`: connection checks and export tool.

The project and overview stay here as the main entry points. Local `.kicad_prl` settings are Git-ignored.

**Draft:** [Pin map V2](docs/pin_map.md) uses all 17 application GPIO. Confirm final parts and the shared 5 V / 3 A budget before wiring. PIC32 programming works (user-reported); peripherals are not bench-tested.

[Previews / review](review/README.md) · [Integration decisions](docs/design_notes.md) · [Run checks](tools/README.md) · [Purchase list](../purchase_list.md)
