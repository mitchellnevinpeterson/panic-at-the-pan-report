# Power + PIC32 — P6

[Schematic](../sheets/power_pic32.kicad_sch) · [Preview](../review/power_pic32/schematic.svg)

The user's loose PIC32 programs successfully with SNAP. Its existing core wiring is preserved.

| Connection | Circuit |
|---|---|
| 3.3 V | VDD 13, AVDD 28; 100 nF at each supply |
| Ground | VSS 8/19, AVSS 27 |
| VCAP 20 | 10 µF to ground, ≥6.3 V, ESR ≤3 Ω |
| MCLR 1 | 10 kΩ pull-up to 3.3 V |
| J2 programming adapter | MCLR, VDD, GND, PGED1/p4, PGEC1/p5 |
| J3 UART | GND, TX RB7/p16, RX RB6/p15; 3.3 V signals |

C4 is optional/DNP extra bulk capacitance. J2 is the lab adapter, **not the raw SNAP pinout**. MCU numbers apply to SOIC/SPDIP/SSOP, not QFN.

| Source | Route |
|---|---|
| Course 12 V / 5 A | Breadboard regulator → J1 regulated 3.3 V; ~100 mA total allowance |
| Course 5 V / 3 A | J4 → J5/J6 and global `+5V_LOAD`; shared by all 5 V loads |
| Course 12 V / 5 A | Proposed J7 → `+12V_LOAD` for candidate motor driver |

`+3.3V`, `+5V_LOAD`, `+12V_LOAD` and `GNDREF` connect across sheets. Keep the breadboard regulator's 5 V output separate from the course 5 V supply. Route motor returns directly to distribution.

**Pending:** inspect supply connectors/returns and simultaneous ratings; size protection and wiring; close current budgets. [Pin map V2](pin_map.md) assigns all 17 application GPIO, with independent SPI1/SPI2 buses. Firmware initialization is pending.

Source: [Microchip DS60001168L, Table 3 and section 2](https://www.microchip.com/content/dam/mchp/documents/MCU32/ProductDocuments/DataSheets/PIC32MX1XX2XX283644-PIN_Datasheet_DS60001168L.pdf), Lab 2 core and the user's course power slide.
