# Inputs — I3

[Schematic](../sheets/inputs.kicad_sch) · [Preview](../review/inputs/schematic.svg) · **03 Inputs** in the hierarchy.

| Function | Draft circuit |
|---|---|
| Five ingredients + Finish | Six active-low contacts to MCP23S17 GPA0–GPA5; individual 10 kΩ pull-ups to 3.3 V. Firmware debounce. |
| Joystick | Two 3.3 V potentiometer axes; wipers to ADC. Actual module/resistance/terminal order TBD. |
| Finish indicator | MCP GPB0 → 74ACT244 channel 8→12 → 2N7000 Gate. LED target ≤10 mA; lamp/R29 pending. |
| Ultrasonic | Conditional 5 V sensor: trigger buffer, 10 kΩ/15 kΩ echo divider. Owned sensor model still unknown. |
| Pan flip | **GPA6 / MCP pin 27 reserved** for `PAN_FLIP`; J24.1 signal, J24.2 GND. Active-low contact example remains **DNP**; mechanism and rearm rule undecided. No extra PIC GPIO needed. |

**Allocation:** owned MCP23S17 DIP-28 added as U11. Dedicated hardware SPI2; 3.3 V supply, 100 nF bypass, address pins grounded, RESET tied directly to 3.3 V, SS pulled HIGH through 10 kΩ. Switch polling uses no interrupt pin. Whole system: **17 of 17 application GPIO**, with optional flip on the expander. See [physical pin map](pin_map.md). Global signal labels now connect sheets.

Global supplies are `+3.3V`, `+5V_LOAD` and `GNDREF`. Inputs share the ~100 mA breadboard-module allowance on 3.3 V and the supplied 5 V / 3 A load budget. No 12 V input.

## Finish indicator — selected 2026-11-09

- ✅ User selected **2N7000 + 74ACT244 buffer** for the Finish indicator. IRLZ34N is not needed for this small LED load.
- ✅ Drawn: U11 GPB0/p1 → U2 pin 8 → U2 pin 12 → R27 (1 kΩ) → Q10 Gate. U2 pin 8 is no longer grounded; pin 12 is no longer NC.
- ✅ R36 = 10 kΩ buffer-input pull-down; R28 = 100 kΩ Gate pull-down. Firmware must write GPB0's output latch LOW before enabling output mode.
- [ ] Confirm actual lamp voltage/current and R29; verify the actual 2N7000 manufacturer pinout.

**Q10 reference pinout:** 1 Source to GND, 2 Gate, 3 Drain to LED cathode. [onsemi 2N7000](https://www.onsemi.com/download/data-sheet/pdf/2n7000-d.pdf) specifies RDS(on) ≤6 Ω at VGS = 4.5 V. J25 carries the **buffered 5 V Gate signal**, not a PIC or expander input.

Before wiring:

- Confirm joystick resistance: a 10 kΩ pot presents at most 2.5 kΩ source resistance; PIC32 recommends ≤5 kΩ (datasheet Table 30-34). RV20/21 represent internal parts, excluded from PCB/BOM.
- Finish LED target remains ≤10 mA; R29 is TBD. D10 represents the button LED, excluded from PCB/BOM.
- Confirm ultrasonic supply, trigger threshold and echo voltage. HC-SR04 is only a candidate. U10 needs a SOT-23/SC70 adapter; its 5 V TTL-compatible input accepts a 3.3 V trigger. The divider gives 3.0 V from a 5.0 V echo; it is unsuitable without review for a 3.3 V echo. Verify waveform/timing and sensor bypass needs.

## Candidate — LSM303D

User identified this part; not selected or confirmed acquired. It combines a 3-axis accelerometer and magnetometer (no gyroscope), with I²C/SPI interfaces. A proposed use is mounting it on the pan to detect a tilt threshold. Accelerometer-based tilt is most reliable when stationary or moving slowly; a fast flip adds acceleration, so filtering/threshold testing is needed. It is not an ultrasonic distance-sensor replacement. Verify the actual breakout board's supply and logic levels before drawing connections. [ST documentation](https://www.st.com/en/mems-and-sensors/lsm303d.html).

Sources: [TI SN74AHCT1G125](https://www.ti.com/lit/ds/symlink/sn74ahct1g125.pdf), [TI SN74ACT244](https://www.ti.com/lit/ds/symlink/sn74act244.pdf), local PIC32 datasheet.
