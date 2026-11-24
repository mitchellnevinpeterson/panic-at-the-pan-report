# Scaffold validation — 2026-11-07

`make -C firmware check` completed successfully on the development laptop with:

- MPLAB XC32 v6.00, target `32MX170F256B`.
- Microchip PIC32MX_DFP 1.7.380.
- GNU Make from the system toolchain.

All explicitly listed C sources compiled, the target ELF linked, and
`xc32-bin2hex` generated a HEX file. The provenance verifier checked all 34
preserved lab/framework files against their recorded SHA-256 values. No sibling
lab files were modified.

The compiler reported no warnings in the newly authored scaffold code. Preserved
sources reported warnings for unimplemented SPI API parameters/missing returns,
an unused display variable, a font bounds-expression precedence issue, an unused
SPI lookup table, and `#pragma mark` annotations. These remain visible and are
documented in the firmware README. The lab drivers are not called by the scaffold.

The first link exposed the runtime's heap requirement. The Makefile now reserves
1024 bytes of heap and 2048 bytes of stack; the subsequent link completed. These
are provisional build reservations, not measured memory requirements.

No device was flashed. No sensor, display, motor, queue-load, timing, or gameplay
test was performed. The build verifies structural/toolchain integration only.
