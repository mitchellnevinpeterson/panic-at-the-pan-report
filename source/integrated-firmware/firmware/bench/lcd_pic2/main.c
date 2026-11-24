#include "LcdSoftSPI.h"
#include "heat_lcd.h"
#include "heat_audio.h"
#include "st7789.h"
#include "McpInputs.h"

volatile bool LcdBenchReady;
volatile unsigned LcdBenchPhase; /* 0 boot, 1 initializing, 2 sweep, 3 fault. */
volatile uint32_t LcdBenchDistanceMm;
static HeatZone zone = HEAT_SILENT;

/* Reuse the teammate's heat UI without linking its audio/encoder driver. */
HeatZone heat_audio_zone(void) { return zone; }

static void debug_init(void)
{
    LATBSET = 1u << 4; /* PIC2 debug TX11. */
    ODCBCLR = 1u << 4;
    TRISBCLR = 1u << 4;
    TRISBSET = 1u << 6; /* Debug RX15. */
    pps_unlock();
    RPB4R = 1;
    U1RXR = 1;
    pps_lock();
    U1MODE = 0;
    U1STA = 0;
    U1MODEbits.BRGH = 1;
    U1BRG = 42; /* PBCLK20MHz / (4*43), nominal 115200. */
    U1STAbits.UTXEN = 1;
    U1STAbits.URXEN = 1;
    U1MODEbits.ON = 1;
}

static void text(const char *s)
{
    while (*s) {
        uint32_t start = ticks_now();
        while (U1STAbits.UTXBF)
            if ((uint32_t)(ticks_now() - start) >= 20000u) return;
        U1TXREG = (uint8_t)*s++;
    }
}

int main(void)
{
    __builtin_disable_interrupts();
    DDPCONbits.JTAGEN = 0;
    /* Keep connected actuator/sensor signals quiet during this LCD-only test. */
    ANSELBCLR = (1u << 2) | (1u << 15);
    LATBCLR = (1u << 2) | (1u << 15); /* US TRIG6, audio26 LOW. */
    TRISBCLR = (1u << 2) | (1u << 15);
    LATASET = 1u; TRISACLR = 1u; ANSELACLR = 1u; /* MCP SS2 HIGH. */
    debug_init();
    text("\r\nLCDPIC2Bench: MOSI14 SCK16 CS18 DC23 RST24; 115200 baud\r\n"
         "LCD-only simulated heat sweep. No game, UART2, ranging or audio.\r\n");
    bool mcp = McpInputs_Init(); /* Clear retained lamp latches; SPI1 stays separate. */
    text(mcp ? "MCP lamp outputs cleared.\r\n" :
               "MCP unavailable; lamp state unverified. LCD test continues.\r\n");
    display_spi_start();
    st7789_prepare_pins();
    LcdBenchPhase = 1;
    text("Drawing PAN HEAT; allow about 15 seconds.\r\n");
    LcdBenchReady = heat_lcd_start();
    if (!LcdBenchReady) {
        LcdBenchPhase = 3;
        text("LCD drawing failed.\r\n");
        for (;;) { }
    }
    LcdBenchPhase = 2;
    text("LCD commands sent; visually verify. Heat bar sweeps automatically.\r\n");
    int distance = 400, step = -5;
    uint32_t sample = ticks_now(), report = sample;
    for (;;) {
        uint32_t now = ticks_now();
        if ((uint32_t)(now - sample) >= 100000u * CORE_TICKS_PER_US) {
            sample = now;
            LcdBenchDistanceMm = (uint32_t)distance;
            zone = distance < HEAT_HOT_BELOW_MM ? HEAT_HIGH :
                   distance > HEAT_COOL_ABOVE_MM ? HEAT_LOW : HEAT_RIGHT;
            heat_lcd_set_distance((uint32_t)distance);
            heat_lcd_update();
            if (distance == 50) step = 5;
            else if (distance == 400) step = -5;
            distance += step;
        }
        if (U1STAbits.OERR) U1STACLR = _U1STA_OERR_MASK;
        if (U1STAbits.URXDA) { volatile unsigned ignored = U1RXREG; (void)ignored; }
        if ((uint32_t)(now - report) >= 100000000u) {
            report = now;
            text("LCD sweep active; sensor/buttons are not controlling this bench.\r\n");
        }
    }
}
