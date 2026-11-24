# Integration decisions

The five module pages replace the old functional-block overview. Edit native KiCad schematics; module assembly ports are logical names, not physical header numbers.

## Confirmed behavior

- Joystick angle controls the spatula; centering returns it home.
- Pan display: **3–4–3 centered rows**, 10 MAX7219 modules total; 24/32/24 pixels wide, 24 pixels high. Top/bottom inset: 4 pixels per side.
- Score, timer and heat share the Display sheet; auxiliary sizes remain TBD.
- Heat uses an LED bar. The flame mechanism is removed.
- Treat dispenser releases one sealed pouch.
- Pan-flip sensing/animation/bonus remain optional.
- PIC pin map V2 is frozen for the current interfaces. **MCP23S17 GPA6 / pin 27 is reserved for `PAN_FLIP`**, so a future flip contact needs no additional PIC GPIO. GPA7 and GPB1–GPB7 remain spare.
- Finish indicator: **2N7000 + 74ACT244 Gate buffer selected on 2026-11-09**; drawn; lamp/R29 and actual transistor pinout still need confirmation.

## I/O responsibilities

| Controller | Inputs | Outputs |
|---|---|---|
| MCP23S17 expander (SPI2) | Five ingredient switches, Finish switch, optional pan-flip switch | Finish LED control through the 74ACT244 buffer and 2N7000 MOSFET |
| PIC32 direct connections | Joystick X/Y (ADC), ultrasonic ECHO, spatula HOME switch | Displays (SPI1), spatula STEP/DIR/WAKE, treat PWM, audio PWM, ultrasonic TRIG |

Use the expander for ordinary switch inputs. Keep analog inputs and signals requiring precise timing directly on the PIC32. Ultrasonic TRIG is an output; ECHO is an input. The PIC32 reads the expander over SPI2 and updates displays over SPI1; the two peripherals do not communicate directly.

See [pin map V2](pin_map.md) for physical pin numbers. Actuator assignments assume the proposed STEP/DIR driver and treat servo.

## Decisions before wiring

| Item | Current draft / next decision |
|---|---|
| GPIO budget | [Pin map V2](pin_map.md): **17 of 17** application GPIO. SPI1 displays / SPI2 MCP23S17; expander handles six switches, optional flip and Finish indicator. STEP/DIR motor interface remains assumed. |
| Spatula | Proposed stepper + home switch; choose motor torque, travel, mounting and driver. Homing plus step counting is not continuous position feedback. |
| Supplies | Course 12 V / 5 A and 5 V / 3 A. Inspect actual supply; measure shared loads. Breadboard regulator allowance ~100 mA. |
| Display chain | Verify board headers, RSET/current and auxiliary counts; reused four-module driver needs later expansion. |
| Interface buffers | U10/U21 are proposed single-gate parts requiring adapters; spare channels of the owned 74ACT244 could replace them after signal routing is settled. |
| Finish indicator | **2N7000 + approximately 5 V Gate drive via 74ACT244 selected.** Circuit now drawn; confirm lamp current and R29; see [Inputs notes](inputs.md#finish-indicator--selected-2026-11-09). |
| Newly identified candidates | LSM303D for optional pan tilt sensing; TB6612FNG for a possible DC motor approach. Neither is selected or confirmed acquired; see [Inputs](inputs.md#candidate--lsm303d) / [Actuators](actuators.md#candidate--tb6612fng). |
| Other parts | Confirm ultrasonic model, Finish lamp, treat servo and audio board/speaker. |
| Wiring | Choose connectors, protection, wire sizes and flexible pan cable routing. |

Global rails and signal labels connect the sheets. PIC32 pin assignments and MCP23S17 are drawn; firmware initialization remains pending.

Each module's notes link its component references. Original course PDFs remain in the repository; downloaded local datasheets are Git-ignored under `/datasheet/`.
