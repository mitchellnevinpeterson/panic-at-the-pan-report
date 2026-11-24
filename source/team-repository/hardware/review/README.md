# Schematic review

**Updated November 9, 2026:** pin map V2, MCP23S17 and centered 3–4–3 pan display recorded. ERC, cross-sheet connections and affected page renders checked. **Independent verifier: qualified pass, no blocking schematic findings.** Verified physical pins/PPS, separate SPI nets, Finish driver, reset behavior and rendered layouts; no bench tests.

| Preview | Scope |
|---|---|
| [Complete system](overview/schematic.svg) | One zoomable SVG: system connections and all five detailed circuit sheets |
| [KiCad hierarchy](overview/hierarchy.svg) | Native five-module navigation page |
| [Power + PIC32](power_pic32/schematic.svg) | Working core and shared rails |
| [Displays](displays/schematic.svg) | Pan, score, timer, heat |
| [Inputs](inputs/schematic.svg) | Controls, ultrasonic, optional flip |
| [Actuators](actuators/schematic.svg) | Proposed stepper/home and treat servo |
| [Audio](audio/schematic.svg) | Proposed PWM filter and amplifier |

[ERC](system/erc.json): **0 violations**. [Connection checks](system/connections.json): **132 passed**. Pin selections checked against PIC32 ADC/PPS tables and the MCP23S17 DIP pinout.

**Before building:** implement firmware for [pin map V2](../docs/pin_map.md), confirm Finish lamp/R29 and actual 2N7000 pinout, verify the shared 5 V / 3 A budget, and confirm components/connectors. No peripheral bench tests or PCB validation performed.

[Regenerate checks/previews](../tools/README.md). Netlists stay local and Git-ignored.

The complete-system SVG embeds the native KiCad drawings as vectors. Repeated global net labels connect across panels; repeated local labels connect within a panel. Its connection map is checked against the netlist during export. Remaining TBD components are labeled; this overview adds no circuit changes.
