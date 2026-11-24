# PIC32 pin map — V2 — independent SPI buses

PIC32MX170F256B, **28-pin DIP**, top-view pin numbering. Schematic allocation established November 9, 2026; firmware initialization is not implemented.

**Allocation frozen for the current interfaces — November 9, 2026.** Reserve MCP23S17 **GPA6 / pin 27** exclusively for `PAN_FLIP`; adding a flip contact does not require another PIC pin. This freezes the pin map, not component selection or bench validation.

**17/17 application GPIO used.** SPI1 drives displays; SPI2 serves the expander. Finish indicator control moves to expander GPB0. ICSP and UART keep the working lab connections. Actuator reservations assume the proposed STEP/DIR driver and treat servo; a different driver may require revisiting them.

| Function / schematic net | PIC port | Physical pin | Configuration |
|---|---|---:|---|
| Display SS / `DISPLAY_SS` | RA0 | 2 | GPIO output, idle HIGH; manual SS |
| Display MOSI / `DISPLAY_MOSI` | RA1 | 3 | SDO1; RPA1R = 3 |
| Display SCK / `DISPLAY_SCK` | RB14 | 25 | Fixed SCK1 |
| Expander MISO / `EXPANDER_MISO` | RB13 | 24 | SDI2; SDI2R = 3 |
| Expander MOSI / `EXPANDER_MOSI` | RB11 | 22 | SDO2; RPB11R = 4 |
| Expander SCK / `EXPANDER_SCK` | RB15 | 26 | Fixed SCK2; disable lab debug LED |
| Expander SS / `EXPANDER_SS` | RB8 | 17 | GPIO output, idle HIGH |
| Joystick X / `JOY_X_ADC` | RB2 | 6 | ADC AN4, 0–3.3 V |
| Joystick Y / `JOY_Y_ADC` | RB3 | 7 | ADC AN5, 0–3.3 V |
| Ultrasonic trigger / `US_TRIG` | RB12 | 23 | GPIO output |
| Ultrasonic echo / `US_ECHO_3V3` | RA2 | 9 | Digital input; INT2R = 0 if edge interrupt used |
| Spatula STEP | RA4 | 12 | GPIO output, software-timed steps |
| Spatula DIR | RB4 | 11 | GPIO output |
| Spatula WAKE | RB9 | 18 | GPIO output, LOW until ready |
| Spatula HOME | RB10 | 21 | Digital input, active LOW |
| Treat servo / `TREAT_PWM` | RB5 | 14 | OC2; RPB5R = 5 |
| Audio / `AUDIO_PWM` | RA3 | 10 | OC3; RPA3R = 5 |
| SNAP PGED1 | RB0 | 4 | Reserved for programming/debug |
| SNAP PGEC1 | RB1 | 5 | Reserved for programming/debug |
| UART1 RX | RB6 | 15 | U1RXR = 1; retained |
| UART1 TX | RB7 | 16 | RPB7R = 1; retained |

Power, VCAP and MCLR wiring: [Power + PIC32](power_pic32.md).

## MCP23S17 — U11, owned DIP-28

| Function | Expander pin(s) |
|---|---|
| Ingredient 1–5 | GPA0–GPA4: 21–25 |
| Finish switch | GPA5: 26 |
| Reserved optional flip contact / `PAN_FLIP` | GPA6: 27 → J24.1; J24.2 = GND; external circuit DNP |
| Finish LED control | GPB0: 1 → U2 pin 8; U2 pin 12 → R27 → Q10 Gate |
| Spare GPIO | GPA7: 28; GPB1–GPB7: 2–8 |
| VDD / VSS | 9 = 3.3 V; 10 = GND; C22 = 100 nF nearby |
| CS / SCK / SI / SO | 11 / 12 / 13 / 14, respectively |
| A0 / A1 / A2 | 15 / 16 / 17 = GND; address bits 000 |
| RESET | 18; tied directly to 3.3 V; no external reset control |
| INTB / INTA | 19 / 20 unconnected; poll switches |

R34 pulls expander SS HIGH. Six switches retain their external 10 kΩ pull-ups. At MCP power-on/reset, GPB0 is an input: R36 (10 kΩ) holds the buffer input LOW. R28 (100 kΩ) holds the MOSFET Gate LOW; R27 (1 kΩ) limits Gate charging current. On every PIC restart, initialize the expander and explicitly switch the indicator off. PIC reset alone does not reset the MCP; its RESET pin is tied directly to 3.3 V.

**Flip expansion:** the reserved input supports a contact or compatible 3.3 V digital signal. The contact example closes to GND (active LOW), with a pull-up and firmware debounce. Keep the pull-up enabled while the external circuit is unfitted. Final switch geometry, polling/debounce timing and rearm behavior remain open. An analog or I²C/SPI tilt sensor needs a separate interface review. GPA7 and GPB1–GPB7 remain available for eight additional digital signals.

## Firmware requirements before wiring tests

- Use FRC/PLL; disable external/secondary oscillators and JTAG. Set only RB2/RB3 analog; configure allocated digital pins explicitly. Keep `LED_DEBUG` disabled (RB15 is now SCK2).
- **SPI1 → displays only:** start at 100 kHz, mode 0; manual RA0 SS. Send one complete chain-length frame (16 bits per driver, including NO-OPs) before each SS rising edge. SPI1 has no connected MISO; service/discard received data as required to avoid overflow.
- **SPI2 → MCP23S17 only:** start at 100 kHz, mode 0, 8-bit transfers. Use SCK2/RB15, SDO2/RB11, SDI2/RB13 and manual RB8 SS. Hold SS LOW across opcode + register address + data; wait for the final shift to finish before raising SS. Expander operates entirely at 3.3 V.
- Set both SS output latches HIGH before enabling output drivers or SPI clocks. The two buses have separate MOSI, SCK and SS nets; display DOUT is never connected to the expander. Serialize access within each bus; one bus does not clock the other.
- Configure the MCP in BANK=0: GPA0–GPA7 inputs (`IODIRA=0xFF`), GPB0 output and GPB1–GPB7 inputs (`IODIRB=0xFE`). Write `OLATB=0x00` **before** enabling GPB0 output. Enable pull-ups on unused inputs (`GPPUA=0xC0`, `GPPUB=0xFE`); GPA0–GPA5 already have external pull-ups. Keep BANK=0 throughout and reinitialize on every PIC restart. GPA6 is the optional flip contact.
- The reused Lab HAL enables automatic SS and has incomplete SDI/read/SPI2 transfer functions. Implement both transaction drivers before bench testing. **No GPIO/PPS/SPI initialization has been implemented by this schematic update.**
- Reserve Timer2 for OC2 servo PWM and Timer3 for OC3 audio PWM. Proposed Timer4 timestamps + INT2 for echo; Timer5 scheduling for STEP. Core timer remains the ES tick. These are resource reservations, not implemented timing code.

## Changes from V1 — rewire before testing

| Signal | Old PIC pin | New connection |
|---|---:|---|
| Expander SI / MOSI | 3 (shared) | PIC 22 → MCP pin 13 |
| Expander SCK | 25 (shared) | PIC 26 → MCP pin 12 |
| Expander SO / MISO | 14 | MCP pin 14 → PIC 24 |
| Expander SS | 17 | Unchanged: PIC 17 → MCP pin 11 |
| Ultrasonic TRIG | 26 | PIC 23 |
| Treat PWM | 24 | PIC 14, OC2 |
| Finish LED control | 22 | MCP GPB0/pin 1 → 74ACT244 pin 8 → pin 12 → Gate resistor |

Display physical pins stay **3 MOSI / 25 SCK / 2 SS**. Names are now `DISPLAY_MOSI`, `DISPLAY_SCK`, `DISPLAY_SS`; the expander uses matching `EXPANDER_*` names. R29 remains TBD from the actual indicator; verify the actual 2N7000 manufacturer pinout before fitting.

Sources: local PIC32 **DS60001168L**, Table 3 and Tables 11-1/11-2; [MCP23S17 datasheet](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP23017-Data-Sheet-DS20001952.pdf); [MAX7219 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/max7219-max7221.pdf).
