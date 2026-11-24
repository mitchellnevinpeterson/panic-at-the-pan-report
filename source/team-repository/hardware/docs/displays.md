# Displays — D4

[Schematic](../sheets/displays.kicad_sch) · [Preview](../review/displays/schematic.svg)

- **Pan:** **3–4–3 centered rows = 10 MAX7219 modules**, 640 LEDs. Row widths: 24 / 32 / 24 pixels; height: 24 pixels. Top/bottom rows inset 4 pixels from each edge. Inventory pending.
- **Score, timer, heat:** proposed extra MAX7219 banks; counts/layout TBD.
- **Three PIC32 signals:** SCK, DATA, **SS** (MAX7219 calls it LOAD).
- **U2, owned 74ACT244:** 5 V supply; display channels 2→18, 4→16, 6→14; Finish LED channel 8→12. Enables and unused inputs 11/13/15/17 grounded; unused outputs open. C5 = 100 nF.
- Chain: pan row 1 (3) → row 2 (4) → row 3 (3) → score → timer → heat. Common SCK/SS; each bank receives separate power wiring.

J10's harness order is proposed. Assembly symbols use logical ports, not physical header numbers. PIC mapping: SCK RB14/p25, DATA RA1/p3, SS RA0/p2. [Pin map V2](pin_map.md): displays use SPI1; expander uses separate SPI2 clocks/data. Always send a complete display-chain frame before latching. R4 now pulls SS HIGH to 3.3 V; R2/R3 still pull SCK/DATA LOW.

**Power remains unresolved:** `+5V_LOAD` joins the course 5 V / 3 A source across sheets, shared with all other 5 V loads. Ten modules can still challenge the shared 3 A budget at high LED current. Measure the actual boards at the intended brightness before confirming capacity. Never use the ~100 mA breadboard supply for displays.

The reused firmware handles four modules; expanding the chain is future work. Verify module bypass capacitors and connector labels before wiring.

Sources: [TI 74ACT244](https://www.ti.com/lit/ds/symlink/sn74act244.pdf), [MAX7219](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX7219-MAX7221.pdf).

Use a 32×24 logical canvas: top/bottom rows have valid x = 4–27, middle row x = 0–31 (zero-based). Mask absent corner pixels. Physical orientation and DIN/DOUT order must be verified before implementing pixel mapping. Pan-only frames contain 10 × 16 = 160 bits; add one 16-bit word per auxiliary driver in the shared chain.

The 3-module rows require individual modules or suitably separable assemblies; do not assume existing 4-module PCBs can be cut. Firmware is still the reused 4-module draft.
