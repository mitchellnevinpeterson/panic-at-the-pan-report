# Build reference


Use GNU Make, Python 3, MPLAB XC32, and a Microchip PIC32MX Device Family Pack.
The target is `32MX170F256B`.

```sh
make -C firmware check
```

Run that command from the project root. The Makefile finds `xc32-gcc` and
`xc32-bin2hex` on `PATH` and defaults to the last sorted PIC32MX DFP directory
under `~/.mchp_packs/Microchip/PIC32MX_DFP/`. If several pack versions exist,
select one explicitly; the automatic sorting is not semantic version ordering.

```sh
make -C firmware check \
  XC32_CC=/path/to/xc32/bin/xc32-gcc \
  XC32_BIN2HEX=/path/to/xc32/bin/xc32-bin2hex \
  DFP_ROOT=/path/to/Microchip/PIC32MX_DFP/version
```

The DFP folder must contain `xc32/`. The source list and include paths are
project-relative; no teammate's absolute paths or generated MPLAB workspace
files are copied. Compiler executables should be on a path without spaces.
Changing compiler/DFP/build flags requires `make -C firmware clean` first.

Outputs are `firmware/build/panic_scaffold.elf` and `.hex`. This build does **not**
flash a PIC32. The image is a structural integration check, not a hardware test.
The course startup configures UART1 and the core timer even though project I/O
remains unassigned. See the inherited pin reservations in [BoardConfig.h](../config/BoardConfig.h).

`HEAP_SIZE=1024` and `STACK_SIZE=2048` are initial linker reservations, not measured
runtime requirements. There is no application dynamic allocation. The inherited
runtime/libc needs a nonzero heap reservation to link. Reassess memory after the
game is implemented, using the generated map and on-target measurements.
