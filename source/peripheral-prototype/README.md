# PIC32 display and ultrasonic bring-up

The display and HC-SR04 share one PIC32MX170F256B in the 28-pin DIP package. Use **Program E-Paper Display (SNAP)** in VS Code and choose `Display_UC8151D` if prompted. The sensor-only image remains available for bench testing. Both use the Lab 2 clock setup of 40 MHz SYSCLK and 20 MHz PBCLK.

See [LCD wiring and bench test](LCD-SETUP.md) for the ST7789 and dual-display firmware.

## Display board

Use `build/firmware/Display_UC8151D.hex`. The orange ribbon in the photo is marked `HINK-E029A10-A3`, which matches the UC8151D/Z13 driver identification. The program draws a burger, pizza slice, apple, and ice cream cone, waits for the refresh to finish, and powers down the panel controller. Edit the pixel patterns in `draw_food_icons()` in `src/display.c` to change the icons. The two image planes use 9,472 bytes of PIC32 RAM.

The display shows the latest ultrasonic reading in whole centimeters beneath the icons. `--- cm` indicates an invalid reading. After the initial update, the firmware waits at least 180 seconds between full refreshes and refreshes only when the displayed value changes. Serial readings continue between screen updates. [Adafruit refresh guidance](https://learn.adafruit.com/adafruit-2-9-eink-display-breakouts-and-featherwings/circuitpython-usage-tricolor)

| Adafruit breakout label | Connect to |
|---|---|
| VIN | 3.3 V supply |
| GND | PIC32 ground |
| SCK | RB14, DIP 25 |
| MOSI | RA1, DIP 3 |
| ECS | RA0, DIP 2 |
| D/C | RB7, DIP 16 |
| RST | RB8, DIP 17 |
| BUSY | RB9, DIP 18 |
| SRCS and SDCS | Pull each high to the 3.3 V rail through 10 kΩ |
| MISO, 3V3, ENA | Leave open |

Use the labeled breakout pads. Give the first refresh up to 45 seconds. SPI1 is mode 0 at 1 MHz. With a logic analyzer, check ECS, D/C, SCK, and MOSI. BUSY should go low during refresh and return high when ready. If using MPLAB Watch, `display_stage == 5` means the program reached the end of its refresh and power-down sequence. The `spi_timeout` and `busy_timeout` flags identify stalled operations.

## Ultrasonic sensor on the display board

| Connection | Connect to |
|---|---|
| HC-SR04 VCC | 5 V supply |
| HC-SR04 GND | PIC32 ground |
| HC-SR04 TRIG | RB2, DIP 6, through a 3.3 V to 5 V digital buffer |
| HC-SR04 ECHO | RB3, DIP 7, through the divider below |
| Optional serial output | RB4, DIP 11, to RX on a 3.3 V USB-to-UART adapter |
| Serial adapter GND | PIC32 ground |

Connect HC-SR04 ECHO to 10 kΩ, the resistor junction to RB3, and 15 kΩ from the junction to ground. This reduces a 5 V echo to about 3.0 V.

You can use one channel of the Lab 2 74ACT244 to shift TRIG. For the 20-pin DIP, power pin 20 at 5 V and ground pin 10. Ground pin 1 to enable the first bank, wire RB2 to pin 8, and wire pin 12 to HC-SR04 TRIG. Tie the other bank's enable, pin 19, to 5 V, tie unused inputs to ground, and place a 0.1 µF capacitor across the buffer supply. Add a 10 kΩ pulldown on the TRIG buffer input if the sensor triggers while the PIC32 is in reset.

Open the serial adapter at **9600 baud, 8N1**. Use only RX and ground on the adapter. Place a flat target about 20 cm in front of the sensor. Expect roughly 200 mm and an echo near 1,160 µs. Without the serial adapter, use MPLAB Watch for `distance_mm`, `echo_us`, and `range_status`, or probe TRIG and the divided ECHO node. The program sends a 12 µs trigger, applies a 30 ms timeout per echo edge, and waits at least 100 ms between measurement attempts.

For a sensor-only bench test, `build/firmware/Ultrasonic_HCSR04.hex` uses the original wiring of RB7 (DIP 16) for TRIG, RB8 (DIP 17) for the divided ECHO, and RA0 (DIP 2) for serial TX.

## Heat zones and speaker output

The speaker plays an original 32-bar arcade loop (about 60 seconds per repeat). The music stays in front of the frying sounds and dips for the heat chimes.

The LCD's EC11 encoder can control the overall speaker volume. With power off, connect encoder A to RA2 (DIP 9), B to RA3 (DIP 10), and encoder common to PIC32 ground. These are the proposed PIC32 pins for a passive encoder or a 3.3 V-compatible encoder breakout. Check the module's encoder labels before connecting. Remove any old ground jumpers from DIP pins 9 and 10. Leave the encoder's push switch open.

The combined firmware treats a smaller distance as more heat. Below 150 mm it plays an intense crackle with bursts. From 150 to 280 mm it plays a gentle frying sound with soft oil pops. Above 280 mm it plays sparse oil pops. Each zone starts with a short cue: a descending pair for low heat, an ascending pair for just right, and two high beeps for too much heat. Three consecutive readings in a different zone switch the sound, and three invalid readings mute it. The UART prints a `HEAT:` line whenever the zone changes. Adjust the distance limits in `src/heat_audio.c` and the sound profiles in `src/sizzle.h`.

The PIC32 generates audio on RB15 (DIP 26). For a powered speaker with an analog input, wire pin 26 through a 4.7 kΩ resistor to a junction, then put a 10 nF capacitor from the junction to common ground. Connect the junction through a 1 µF nonpolar capacitor to the speaker's analog input, and tie the input ground to PIC32 ground. Power the speaker from its rated supply. Use an audio amplifier between this circuit and a bare speaker.

## Program with MPLAB IPE

Use the same SNAP and ICSP wiring as Lab 2. This image uses PGED1 on RB0 (DIP 4) and PGEC1 on RB1 (DIP 5). Connect MCLR (DIP 1), target voltage sense, and common ground. Power the PIC32 circuit externally at 3.3 V. Connect and program one target at a time.

1. Select PIC32MX170F256B and your SNAP in MPLAB IPE, then connect.
2. Browse to the appropriate HEX file under `build/firmware`.
3. Select **Program** and wait for verification. Run or reset the programmed target.

Build again from this folder with `cmake -S . -B build` followed by `cmake --build build`. The default XC32 and DFP paths point to the Lab 2 installation on this Mac.

## Hardware references

- [Microchip PIC32MX1XX/2XX family datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/PIC32MX1XX2XX%20283644-PIN_Datasheet_DS60001168L.pdf)
- [Adafruit breakout pinouts](https://learn.adafruit.com/adafruit-2-9-eink-display-breakouts-and-featherwings/pinouts)
- [Adafruit UC8151D command definitions and tri-color initialization](https://github.com/adafruit/Adafruit_EPD/blob/master/src/drivers/Adafruit_UC8151D.h)
- [GxEPD2 panel identification](https://github.com/ZinggJM/GxEPD2/blob/master/examples/GxEPD2_Example/GxEPD2_display_selection_new_style.h)
- [ElecFreaks HC-SR04 timing sheet](https://www.digikey.com/htmldatasheets/production/3822706/0/0/1/hc-sr04.pdf)
- [TI 74ACT244 datasheet](https://www.ti.com/lit/ds/symlink/sn74act244.pdf)
