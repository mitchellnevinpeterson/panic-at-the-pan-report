#include <xc.h>
#include <cp0defs.h>
#include "Servo50Hz.h"
#include <stdio.h>

/* Timer2 1:8 from PBCLK 20 MHz = 2.5 MHz (0.4 us/tick); 50000 ticks = 20 ms.
 * PPS Table 11-2 (DS60001168L) and the DFP .PIC file: RPA4 is in the output
 * group where 0101 = OC4, RPB5 in the group where 0101 = OC2.
 * Neither RA4 nor RB5 has an ANSEL bit (ANSELA = RA0/1, ANSELB = RB0-3/12-15).
 */
#define SPOON_PIN (1u << 4)     /* RA4, pin 12 */
#define TREAT_PIN (1u << 5)     /* RB5, pin 14 */
#define PPS_OC4 5u
#define PPS_OC2 5u
#define PERIOD_TICKS 50000u
#define OC_PWM_TIMER2 6u        /* OCM=110 PWM, fault off, OCTSEL=0 (Timer2), 16-bit */
#define ROUTE_ATTEMPTS 3u

static bool initialized, on[2];

static bool route(void)
{
    /* Caller holds interrupts off so nothing can split the unlock sequence. */
    SYSKEY = 0;
    SYSKEY = 0xAA996655;
    SYSKEY = 0x556699AA;
    CFGCONbits.IOLOCK = 0;
    RPA4R = PPS_OC4;
    RPB5R = PPS_OC2;
    CFGCONbits.IOLOCK = 1;
    SYSKEY = 0;
    return (RPA4R & 15u) == PPS_OC4 && (RPB5R & 15u) == PPS_OC2;
}

void Servo50Hz_Init(void)
{
    unsigned int interrupts = __builtin_disable_interrupts();
    unsigned int attempt;
    PMD3CLR = _PMD3_OC2MD_MASK | _PMD3_OC4MD_MASK; /* PMDL1WAY=OFF, PMDLOCK=0 */
    PMD4CLR = _PMD4_T2MD_MASK;
    OC2CON = 0;
    OC4CON = 0;
    T2CON = 0;
    LATACLR = SPOON_PIN;
    ODCACLR = SPOON_PIN;
    TRISACLR = SPOON_PIN;
    LATBCLR = TREAT_PIN;
    ODCBCLR = TREAT_PIN;
    TRISBCLR = TREAT_PIN;
    for (attempt = 0; attempt < ROUTE_ATTEMPTS && !route(); ++attempt) { }
    IEC0CLR = _IEC0_T2IE_MASK | _IEC0_OC2IE_MASK | _IEC0_OC4IE_MASK;
    IFS0CLR = _IFS0_T2IF_MASK | _IFS0_OC2IF_MASK | _IFS0_OC4IF_MASK;
    T2CONbits.TCKPS = 3;
    PR2 = PERIOD_TICKS - 1u;
    TMR2 = 0;
    /* Duty 0 keeps a PWM output LOW. OCxR is loaded only here; afterwards only
     * the double-buffered OCxRS changes, so every pulse starts at a period
     * boundary and is complete (no runt pulse on enable/disable). */
    OC2R = 0;
    OC2RS = 0;
    OC4R = 0;
    OC4RS = 0;
    OC2CON = OC_PWM_TIMER2;     /* configured, still OFF: pins follow LAT (LOW) */
    OC4CON = OC_PWM_TIMER2;
    T2CONSET = _T2CON_ON_MASK;  /* runs forever; channels never touch Timer2 */
    on[SERVO_SPOON] = false;
    on[SERVO_TREAT] = false;
    initialized = true;
    if (interrupts & 1u) {
        __builtin_enable_interrupts();
    }
}

bool Servo50Hz_Write(Servo50Hz_Channel channel, uint16_t pulse_us)
{
    uint32_t ticks;
    if (!initialized || (unsigned int)channel > SERVO_TREAT ||
        pulse_us < SERVO50HZ_MIN_US || pulse_us > SERVO50HZ_MAX_US) {
        return false;
    }
    ticks = ((uint32_t)pulse_us * 5u + 1u) / 2u; /* 0.4 us/tick, rounded */
    if (channel == SERVO_SPOON) {
        OC4RS = ticks;
        if (!on[SERVO_SPOON]) OC4CONSET = _OC4CON_ON_MASK; /* OC4R=0: LOW until rollover */
    } else {
        OC2RS = ticks;
        if (!on[SERVO_TREAT]) OC2CONSET = _OC2CON_ON_MASK;
    }
    on[channel] = true;
    return true;
}

void Servo50Hz_Disable(Servo50Hz_Channel channel)
{
    /* Return the channel to its exact post-Init state (OC off, OCxR = OCxRS = 0),
     * so the next Write re-enables through the same path as the first move.
     * The earlier "0% duty, module left on" stop was unverified on this chip,
     * and on the bench a later move sometimes did nothing. Wait (<= 2.5 ms)
     * for the pulse in flight to end so the servo never sees a runt pulse.
     * Timer2 keeps running, so the other channel is untouched. */
    uint32_t start;
    if (!initialized || (unsigned int)channel > SERVO_TREAT || !on[channel]) return;
    start = _CP0_GET_COUNT();
    while (TMR2 < (channel == SERVO_SPOON ? OC4R : OC2R) &&
           (uint32_t)(_CP0_GET_COUNT() - start) < 60000u) { } /* 3 ms guard at 20 MHz */
    if (channel == SERVO_SPOON) {
        OC4CONCLR = _OC4CON_ON_MASK;
        OC4R = 0;
        OC4RS = 0;
    } else {
        OC2CONCLR = _OC2CON_ON_MASK;
        OC2R = 0;
        OC2RS = 0;
    }
    on[channel] = false;
}

bool Servo50Hz_Routed(void)
{
    return initialized && (RPA4R & 15u) == PPS_OC4 && (RPB5R & 15u) == PPS_OC2;
}

void Servo50Hz_Report(void)
{
    /* Pulse width = OCxRS * 0.4 us (2000 us -> 5000). PORTB5 is one instant sample. */
    printf("SERVO T2CON=%04lX PR2=%lu TMR2=%lu | TREAT OC2CON=%04lX OC2R=%lu OC2RS=%lu (%lu us) "
           "RPB5R=%lu TRISB5=%u LATB5=%u PIN14=%u on=%u | SPOON OC4CON=%04lX OC4RS=%lu RPA4R=%lu on=%u\r\n",
           (unsigned long)(T2CON & 0xFFFFu), (unsigned long)PR2, (unsigned long)TMR2,
           (unsigned long)(OC2CON & 0xFFFFu), (unsigned long)OC2R, (unsigned long)OC2RS,
           (unsigned long)(OC2RS * 2u / 5u), (unsigned long)(RPB5R & 15u),
           (unsigned)((TRISB >> 5) & 1u), (unsigned)((LATB >> 5) & 1u), (unsigned)((PORTB >> 5) & 1u),
           (unsigned)on[SERVO_TREAT], (unsigned long)(OC4CON & 0xFFFFu), (unsigned long)OC4RS,
           (unsigned long)(RPA4R & 15u), (unsigned)on[SERVO_SPOON]);
}
