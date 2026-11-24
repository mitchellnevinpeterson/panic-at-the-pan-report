# Panic at the Pan — preliminary purchase list

**Historical estimate — not an order list.** The flame mechanism was removed, and the spatula now follows joystick angle. The old motor choices and totals below are superseded; see [current module decisions](hardware/docs/design_notes.md). Reprice after the motor and GPIO architecture are selected.

Original estimate: November 7, 2026. Purchase status includes items already on hand.

## Confirmed direction

- Reuse the PIC32MX170F256B, MAX7219 LED panels, and basic resistors/capacitors already available.
- Display ingredient icons and swirl animations on the existing monochrome panels. No new LEDs in the baseline.
- A joystick selects the direction of circular spoon motion. A neutral/stop region and actual stirring speed still need calibration.
- A printed flame rises through a slot. One sealed Gushers pouch drops after a completed game.
- Available supply voltages: 3.3 V and 5 V. **Current capacity and supply model remain unknown.**

The course limit is $200 of team-funded materials at market value, excluding lab-kit and Cabinet of Freedom materials. The finished machine must run connected only to the provided project supplies. See [course specification](ME218a_F26_Project_Spec_V2.pdf). Do not assume a new wall adapter is permitted.

## Reuse / inventory before buying

| Item | Status | Check |
|---|---|---|
| PIC32MX170F256B | Confirmed owned | Reuse the working core, programming connection and decoupling from Lab 2. |
| MAX7219 displays | ☐ Availability pending (latest update) | Lab 3 uses four 8×8 modules per 32×8 board. Pan now needs 10 modules in centered 3–4–3 rows. Verify individual/separable modules for 3-module rows; allocate score, timer and heat separately. |
| Ultrasonic sensor | Confirmed owned; several available | Identify the model and verify its supply and signal levels before wiring. |
| Resistors and capacitors | Confirmed owned | Check needed values, capacitor types, voltage ratings and any motor bulk capacitance separately. |
| Wiring / distribution | ✅ Provided by lab | Servo leads, switch terminals, power wire, terminal blocks, headers, heat-shrink and strain relief; select appropriate sizes and ratings for the final loads. |
| MCP23S17 DIP-28 | ✅ Confirmed owned | Dedicated SPI2 digital I/O expander; also controls Finish LED through the buffer/MOSFET. See [pin map](hardware/docs/pin_map.md). No MCP23017 purchase needed. |
| 74ACT244 | ✅ Confirmed owned | Reuse for 3.3-to-5 V display signals and possibly the ultrasonic trigger. Do not substitute a 74HC244 without checking input thresholds. |
| SNAP programmer, USB-UART, breadboard, jumpers | Used in labs; inventory unconfirmed | Verify availability. Lab 3 uses a CP2102 USB-UART. These are development tools, not extra game controllers. |
| 3.3 V / 5 V power hardware | Voltages confirmed | Read current ratings on BOTH the source and any breadboard regulator. A voltage label alone does not establish motor-driving capacity. |

## Candidate purchases and budget

Exact dollar amounts are vendor listings checked during this session; availability, tax, and shipping need checking at checkout. Rows marked **allowance** are planning estimates, not vendor quotes. Actuator choices remain conditional on loads and dimensions.

**Purchased?**: ✅ = purchased or already on hand; ☐ = still to acquire or confirm.

| Item | Purchased? | Qty | Candidate / selection requirement | Unit / allowance | Extended |
|---|:---:|---:|---|---:|---:|
| Ingredient switches | ☐ | 5 | Panel-mount toggle or spring-return levers; contact-only SPST/SPDT is sufficient. Exact feel and model TBD. | $3 allowance | $15.00 |
| Finish button | ☐ | 1 | [Adafruit 3491, illuminated 30 mm](https://www.adafruit.com/product/3491); switch and LED have separate terminals | $2.50 | $2.50 |
| Analog joystick | ☐ | 1 | [Adafruit 3102, two 10 kΩ potentiometers](https://www.adafruit.com/product/3102); use one axis initially. Mount securely; avoid a long unsupported handle loading its shaft. | $19.95 | $19.95 |
| Ultrasonic sensor | ✅ | 1 | Already available; exact model and electrical requirements to confirm. HC-SR04 was only the original candidate. | Already owned | $0.00 |
| Spoon continuous-rotation servo | ☐ | 1 | [FEETECH FS5106R, Pololu 3430](https://www.pololu.com/product/3430); conservative standard-size candidate. Select after spoon radius/load are known. | $24.35 | $24.35 |
| Flame positioning servo | ☐ | 1 | [FEETECH FS90, Pololu 2818](https://www.pololu.com/product/2818); candidate for a lightweight guided cutout and short linkage | $8.40 | $8.40 |
| Treat positioning servo | ☐ | 1 | [FEETECH FS5106B, Pololu 3425](https://www.pololu.com/product/3425); provisional one-servo separating mechanism | $22.58 | $22.58 |
| Audio amplifier | ☐ | 1 | [PAM8302, Adafruit 2130](https://www.adafruit.com/product/2130); takes a PIC32-generated, filtered audio waveform | $3.95 | $3.95 |
| Speaker | ☐ | 1 | [Adafruit 3351, enclosed 4 Ω / 3 W](https://www.adafruit.com/product/3351); candidate match for PAM8302 | $3.95 | $3.95 |
| Interface components | ☐ | 1 set | Transistor for Finish light, any missing level buffer, audio filter parts and bulk capacitors; subtract lab-stock parts | $5 allowance | $5.00 |
| Mechanical materials | ☐ | 1 set | Enclosure/pan, lightweight spoon, shaft support/bushing, brackets, flame guides, linkage, pouch magazine/chute, fasteners and diffuser | $25 allowance | $25.00 |
| **Hardware/material subtotal** | | | | | **$130.68** |
| Tax and shipping reserve | | | Planning reserve, not a shipping quote | | **$25.00** |
| **Planned cash requirement** | | | | | **$155.68** |
| **Remaining within $200** | | | Must cover treats, any extra actuator, power conversion and overruns | | **$44.32** |

This total estimates remaining purchases and assumes the existing ultrasonic sensor, displays, core electronics, lab-provided wiring/distribution and supplied power can be reused. If the sensor was bought with team funds rather than supplied by the lab, include its original cost in the course budget ledger. It does **not** establish that their power capacity is adequate. A second $22.58 dispenser servo would leave $21.74 of the planning reserve. Packaging samples and presentation-day treats are not priced yet; reserve money for them and confirm how the course counts consumables.

## Conditional alternatives — do not buy both

- **Smaller spoon motor:** [FS90R, Pololu 2820](https://www.pololu.com/product/2820), $9.82, saves $14.53 if testing shows it can turn the lightweight spoon mechanism. FS90R is continuous rotation; FS90 is limited-angle positioning. They are not interchangeable for the intended motions.
- **Extra dispenser actuator:** only if a linked single-servo mechanism cannot reliably separate one flexible pouch. Count the extra signal pin, peak current and budget.
- **Input expansion:** ✅ Owned MCP23S17 DIP-28 selected for six switches and optional flip; no additional expander purchase planned.
- **Color lighting:** optional aesthetic upgrade after the monochrome prototype works. No RGB matrix/strip is included in the budget.
- **Power conversion:** determine whether any additional converter is needed after source current and motor peak currents are known. Wiring and distribution materials are provided by the lab. Do not buy a larger external supply based on this list.

## Mechanical selection notes

1. **Spoon:** an offset arm moves the spoon around a circle. Provide support for the arm instead of making the servo shaft carry a large cantilever. Required torque depends on drag, radius and acceleration, not only spoon mass. Direction control does not require shaft-position feedback. The continuous servo has a neutral pulse and must be calibrated to stop.
2. **Flame:** one positioning servo can raise a light cutout with a crank/linkage. Use guides so it cannot rotate into the slot. Choose stroke and linkage geometry before deciding whether a micro servo suffices. No independent door motor in this version.
3. **Treat:** first mock up a magazine and one-pouch separating mechanism using real sealed pouches. A flap under an unseparated pile is not a reliable one-pouch dispenser. Motor torque and whether a second gate is necessary remain open.
4. **Sound:** baseline is synthesized sound from the PIC32 plus amplifier/speaker. The amplifier cannot generate or store sounds. Recorded audio playback hardware is outside this baseline.

## Order-readiness checklist

- [ ] Record 5 V source and regulator current ratings; verify the course-supplied configuration.
- [ ] Count and measure existing panels; choose the number of panels for the pan.
- [ ] Choose pan diameter, spoon orbit radius, flame travel and pouch magazine capacity.
- [ ] Choose ingredient switch feel: maintained toggle or spring-return.
- [ ] Confirm that Gushers have teaching-team approval, as required by the course.
- [ ] Check vendor stock and delivery dates before paying; listed price does not guarantee arrival before the November 13 checkpoint.
- [ ] Recompute the complete bill at checkout, including mechanics and incidental supplies.

See [hardware planning notes](hardware/README.md) and the [editable KiCad interface draft](hardware/panic_at_the_pan_interfaces.kicad_sch).
