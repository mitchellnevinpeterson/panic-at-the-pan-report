#include <xc.h>
#include "SpoonServo.h"

#define SIGNAL_MASK (1u << 4)
static bool initialized, enabled;

void SpoonServo_Init(void)
{
    unsigned int interrupts = __builtin_disable_interrupts();
    SYSKEY = 0;
    SYSKEY = 0xAA996655;
    SYSKEY = 0x556699AA;
    CFGCONbits.IOLOCK = 0;
    PMD3bits.OC4MD = 0;
    PMD4bits.T2MD = 0;
    OC4CON = 0;
    T2CON = 0;
    RPA4R = 5;             /* OC4 on RA4, PPS Table 11-2. */
    CFGCONbits.IOLOCK = 1;
    SYSKEY = 0;
    LATACLR = SIGNAL_MASK;
    ODCACLR = SIGNAL_MASK;
    TRISACLR = SIGNAL_MASK; /* RA4 has no analog function. */
    IEC0CLR = _IEC0_T2IE_MASK | _IEC0_OC4IE_MASK;
    IFS0CLR = _IFS0_T2IF_MASK | _IFS0_OC4IF_MASK;
    T2CONbits.TCKPS = 3;   /* 1:8 prescaler, 20 MHz / 8 = 2.5 MHz. */
    PR2 = 49999;           /* 50000 ticks = 20 ms / 50 Hz. */
    TMR2 = 0;
    OC4R = 3750;
    OC4RS = 3750;
    OC4CON = 6;            /* PWM, Timer2, 16-bit, fault input disabled. */
    enabled = false;
    initialized = true;
    if (interrupts & 1u) {
        __builtin_enable_interrupts();
    }
}

bool SpoonServo_WritePulse(uint16_t pulse_us)
{
    uint32_t ticks;
    if (!initialized || pulse_us < SPOON_SERVO_MIN_US || pulse_us > SPOON_SERVO_MAX_US) {
        return false;
    }
    ticks = ((uint32_t)pulse_us * 5u + 1u) / 2u; /* 0.4 us/tick, rounded. */
    OC4RS = ticks;          /* Double-buffered: applies at the next PWM period. */
    if (!enabled) {
        OC4R = ticks;      /* Initial pulse; never rewrite OC4R while running. */
        TMR2 = 0;
        OC4CONSET = _OC4CON_ON_MASK;
        T2CONSET = _T2CON_ON_MASK;
        enabled = true;
    }
    return true;
}

void SpoonServo_Disable(void)
{
    if (initialized) {
        OC4CONCLR = _OC4CON_ON_MASK;
        T2CONCLR = _T2CON_ON_MASK;
        LATACLR = SIGNAL_MASK;
        enabled = false;
    }
}
