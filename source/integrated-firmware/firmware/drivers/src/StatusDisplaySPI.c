#include <xc.h>
#include <cp0defs.h>
#include "StatusDisplaySPI.h"

#define MOSI_MASK (1u << 11)
#define SS_MASK   (1u << 13)
#define SCK_MASK  (1u << 15)
#define CORE_TICKS_PER_US 20u
#define WORD_TIMEOUT_TICKS (5000u * CORE_TICKS_PER_US)

static uint16_t transfer[8];
static uint8_t word_count, next_word;
static uint32_t word_started;
static StatusDisplaySPI_State state = STATUS_SPI_FAULT;

void StatusDisplaySPI_Init(void)
{
    unsigned int interrupts = __builtin_disable_interrupts();
    SYSKEY = 0;
    SYSKEY = 0xAA996655;
    SYSKEY = 0x556699AA;
    CFGCONbits.IOLOCK = 0;
    PMD5bits.SPI2MD = 0;
    SPI2CON = 0;
    SPI2CON2 = 0;
    RPB11R = 4;             /* SDO2, per PPS Table 11-2. */
    RPB13R = 0;             /* SS is GPIO, never automatic per-word SS. */
    CFGCONbits.IOLOCK = 1;
    SYSKEY = 0;

    ANSELBCLR = MOSI_MASK | SS_MASK | SCK_MASK;
    ODCBCLR = MOSI_MASK | SS_MASK | SCK_MASK;
    LATBSET = SS_MASK;      /* Set idle values before enabling outputs. */
    LATBCLR = MOSI_MASK | SCK_MASK;
    TRISBCLR = MOSI_MASK | SS_MASK | SCK_MASK;
    (void)SPI2BUF;
    SPI2STATCLR = _SPI2STAT_SPIROV_MASK;
    SPI2BRG = 99;           /* 20 MHz / (2 * (99 + 1)) = 100 kHz. */
    /* Mode 0: CKP=0, CKE=1. 16 bits, standard buffer, no SDI pin.
     * All framed/audio/automatic-SS modes stay disabled.
     */
    SPI2CON = _SPI2CON_MSTEN_MASK | _SPI2CON_CKE_MASK |
              _SPI2CON_MODE16_MASK | _SPI2CON_DISSDI_MASK;
    SPI2CONSET = _SPI2CON_ON_MASK;
    state = STATUS_SPI_IDLE;
    if (interrupts & 1u) {
        __builtin_enable_interrupts();
    }
}

bool StatusDisplaySPI_Start(const uint16_t *words, uint8_t count)
{
    uint8_t i;
    if (state != STATUS_SPI_IDLE || words == 0 || count == 0 || count > 8) {
        return false;
    }
    for (i = 0; i < count; ++i) {
        transfer[i] = words[i];
    }
    word_count = count;
    next_word = 1;
    LATBCLR = SS_MASK;
    word_started = _CP0_GET_COUNT();
    SPI2BUF = transfer[0];
    state = STATUS_SPI_BUSY;
    return true;
}

StatusDisplaySPI_State StatusDisplaySPI_Poll(void)
{
    uint32_t elapsed;
    if (state != STATUS_SPI_BUSY) {
        return state;
    }
    elapsed = (uint32_t)(_CP0_GET_COUNT() - word_started);
    if (elapsed >= WORD_TIMEOUT_TICKS || SPI2STATbits.SPIROV) {
        /* Stop clocks; keep SS LOW so a partial frame is not latched.
         * Init() is required to recover and reinitialize all eight drivers.
         */
        SPI2CONCLR = _SPI2CON_ON_MASK;
        state = STATUS_SPI_FAULT;
        return state;
    }
    /* The guard avoids checking completion before the first clock starts.
     * SPITBE alone is insufficient: the shift register must also be idle.
     */
    if (elapsed < CORE_TICKS_PER_US || !SPI2STATbits.SPITBE ||
        SPI2STATbits.SPIBUSY) {
        return STATUS_SPI_BUSY;
    }
    if (SPI2STATbits.SPIRBF) {
        (void)SPI2BUF;      /* Discard receive data even though no MISO is wired. */
    }
    if (next_word < word_count) {
        word_started = _CP0_GET_COUNT();
        SPI2BUF = transfer[next_word++];
    } else {
        LATBSET = SS_MASK;  /* Exactly one latch edge after the entire chain. */
        state = STATUS_SPI_IDLE;
    }
    return state;
}
