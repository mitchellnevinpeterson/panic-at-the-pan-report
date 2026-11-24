#include <xc.h>
#include <cp0defs.h>
#include "JoystickADC.h"

#define AXIS_PINS ((1u << 2) | (1u << 3))
#define CORE_TICKS_PER_US 20u
#define CONVERSION_TIMEOUT_TICKS (2000u * CORE_TICKS_PER_US)

static JoystickADC_State state = JOYSTICK_ADC_FAULT;
static uint32_t enabled_at, conversion_started;
static uint16_t pending_x;
static bool reading_y;

void JoystickADC_Init(void)
{
    PMD1bits.AD1MD = 0;
    AD1CON1 = 0;
    IEC0CLR = _IEC0_AD1IE_MASK;
    IFS0CLR = _IFS0_AD1IF_MASK;
    ANSELBSET = AXIS_PINS;
    TRISBSET = AXIS_PINS;
    CNPUBCLR = AXIS_PINS;
    CNPDBCLR = AXIS_PINS;
    AD1CON2 = 0;                /* AVDD/AVSS references, one conversion/buffer. */
    AD1CON3 = (16u << _AD1CON3_SAMC_POSITION) | 9u;
    /* TAD = 2 * (9+1) / 20 MHz = 1 us; acquire for 16 us.
     * A nominal 10k joystick pot has at most 2.5k source resistance.
     */
    AD1CHS = 4u << _AD1CHS_CH0SA_POSITION;
    AD1CSSL = 0;
    AD1CON1 = (7u << _AD1CON1_SSRC_POSITION) | _AD1CON1_ON_MASK;
    /* ASAM=0: software starts sampling; internal counter starts conversion. */
    enabled_at = _CP0_GET_COUNT();
    state = JOYSTICK_ADC_IDLE;
}

static void sample_channel(uint8_t channel)
{
    /* Called only when no sample/conversion is active. SMPI=0 keeps the
     * single result in ADC1BUF0. ASAM=0 prevents a background conversion.
     */
    AD1CHS = (uint32_t)channel << _AD1CHS_CH0SA_POSITION;
    IFS0CLR = _IFS0_AD1IF_MASK;
    AD1CON1CLR = _AD1CON1_DONE_MASK;
    conversion_started = _CP0_GET_COUNT();
    AD1CON1SET = _AD1CON1_SAMP_MASK;
}

bool JoystickADC_Start(void)
{
    if (state != JOYSTICK_ADC_IDLE ||
        (uint32_t)(_CP0_GET_COUNT() - enabled_at) < 10u * CORE_TICKS_PER_US) {
        return false;
    }
    reading_y = false;
    sample_channel(4);
    state = JOYSTICK_ADC_BUSY;
    return true;
}

JoystickADC_State JoystickADC_Poll(uint16_t *x, uint16_t *y)
{
    if (state != JOYSTICK_ADC_BUSY) {
        return state;
    }
    if (x == 0 || y == 0) {
        AD1CON1CLR = _AD1CON1_ON_MASK;
        state = JOYSTICK_ADC_FAULT;
        return state;
    }
    /* Check DONE before the timeout: a foreground stall between start and poll
     * must not discard a conversion that already completed. */
    if (!AD1CON1bits.DONE) {
        if ((uint32_t)(_CP0_GET_COUNT() - conversion_started) >= CONVERSION_TIMEOUT_TICKS) {
            AD1CON1CLR = _AD1CON1_ON_MASK;
            state = JOYSTICK_ADC_FAULT;
            return state;
        }
        return JOYSTICK_ADC_BUSY;
    }
    if (!reading_y) {
        pending_x = (uint16_t)ADC1BUF0;
        reading_y = true;
        sample_channel(5);
        return JOYSTICK_ADC_BUSY;
    }
    *x = pending_x;
    *y = (uint16_t)ADC1BUF0;
    state = JOYSTICK_ADC_IDLE;
    return JOYSTICK_ADC_READY;
}
