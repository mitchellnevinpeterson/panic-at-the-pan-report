# PIC32 heat display setup

With power off, place the PIC32MX170F256B across the breadboard gap. The numbers below are physical pins on the 28-pin DIP, counted counterclockwise from pin 1 next to the notch.

| PIC32 pin or signal | Connection |
|---|---|
| 13 (VDD), 28 (AVDD) | Regulated 3.3 V. Add a 0.1 µF bypass capacitor to ground near each pin. |
| 8, 19 (VSS), 27 (AVSS) | Common ground |
| 20 (VCAP) | 10 µF low-ESR ceramic or tantalum rated at least 6 V to ground. If polarized, the positive end goes to pin 20. Keep the leads short. |
| 1 (MCLR) | 10 kΩ pull-up to 3.3 V and SNAP MCLR |
| 4 (PGED1), 5 (PGEC1) | SNAP PGED and PGEC, respectively |
| SNAP VTG and GND | Target 3.3 V and common ground, respectively. Use an external 3.3 V supply for the board. |

## Connect the 2.4-inch ST7789 LCD

Use the labels printed on the module. This wiring assumes a 320 × 240 ST7789 module with separate CS, DC, and reset pins.

| LCD label | Connection |
|---|---|
| VCC | 3.3 V |
| GND | Common ground |
| SCL / SCK | PIC32 pin 25 (RB14 / SCK1) |
| SDA / MOSI | PIC32 pin 3 (RA1 / SDO1) |
| CS | PIC32 pin 21 (RB10) |
| DC | PIC32 pin 22 (RB11) |
| RES / RST | PIC32 pin 24 (RB13) |
| BLK | Leave open initially. On the matching module the backlight defaults on. If yours stays off, check its backlight pin instructions. |
| A, B | Optional for the screen test. Connect A to pin 9 (RA2) and B to pin 10 (RA3) for the volume knob. |
| PUSH, K0 | Leave open for this firmware. |

The PIC32 enables pull-ups for A and B. If the encoder has a separate common pin, connect it to ground. Remove any existing jumpers that tie PIC32 pins 9 and 10 directly to ground. Use 3.3 V LCD power and logic. The LCD and the e-paper can share SCL and MOSI directly from the PIC32.

## Test the display

1. Power the 3.3 V rail and connect the SNAP. In VS Code, select **Program LCD Bench (SNAP)** in Run and Debug and launch it. In MPLAB IPE, select PIC32MX170F256B and your SNAP, then program `build/firmware/LCD_Bench.hex`.
2. The screen should show **PAN HEAT**, then sweep the heat bar back and forth through blue **NOT ENOUGH**, green **JUST RIGHT**, and red **TOO MUCH**. The sensor, e-paper, and amplifier are optional for this test.
3. If the encoder is connected, turn it down from the initial full volume. The white volume segments should decrease. If knob direction is reversed on your module, swap A and B.

## Run both displays with the sensor and audio

Use **Program Dual Display (SNAP)** or `build/firmware/Display_Dual.hex`. Wire the e-paper, HC-SR04, and amplifier per the project README. The e-paper keeps its own CS on PIC32 pin 2, DC on 16, reset on 17, and BUSY on 18. The LCD shows the same confirmed heat zone used by the speaker. The bar grows as distance decreases, from empty at 40 cm to full at 5 cm. The marks below the bar show the 28 cm and 15 cm zone boundaries. The music, chimes, and quieter Just Right profile are included in the firmware. Turn the LCD knob to adjust the overall audio level.

Build with `cmake -S . -B build` and `cmake --build build`. These commands build production HEX images. The launch presets use the ELF files from that build.

References: [Microchip PIC32MX170F256B pinout and power connections](https://ww1.microchip.com/downloads/en/DeviceDoc/PIC32MX1XX2XX%20283644-PIN_Datasheet_DS60001168L.pdf), [Adafruit ST7789 driver reference](https://github.com/adafruit/Adafruit-ST7735-Library/blob/master/Adafruit_ST7789.cpp), [matching display module header](https://www.hestore.eu/en/prod_10052413.html).
