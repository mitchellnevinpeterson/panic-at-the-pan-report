#include <xc.h>
#include <cp0defs.h>
#include <string.h>
#include "Pic1Board.h"
#include "Pic1Config.h"
#include "GameIO.h"
#include "JoystickADC.h"
#include "SpoonServo.h"
#include "Follower.h"

/* Compile exactly one preserved follower. No bench main/hardware init linked. */
#if PIC1_JOYSTICK_MODE == 1
static const Follower_Config follower_config = {
    0, 512, 1023, 40, 1250, 1500, 1750, 10, false
};
#elif PIC1_JOYSTICK_MODE == 2
static const Follower_Config follower_config = {
    {0,512,1023,false}, {0,512,1023,false}, 64,128,500,1250,1500,1750,10,false
};
#else
#error "Select PIC1_JOYSTICK_MODE 1 (X checkpoint) or 2 (angular experiment)."
#endif

static Follower follower;
static uint32_t last_tick, milliseconds, last_adc, last_motion_post;
static uint16_t last_x, last_y, last_activity_x, last_activity_y;
static bool motor_enabled, initialized, fault;
static uint8_t spi_bytes[32], spi_count, spi_index;
static uint32_t spi_started;
static EInkBusState spi_state;

uint32_t Pic1Board_Now(void)
{
    uint32_t elapsed = (uint32_t)(_CP0_GET_COUNT() - last_tick);
    uint32_t whole = elapsed / 20000u;
    last_tick += whole * 20000u;
    milliseconds += whole;
    return milliseconds;
}

static void reset_pin(bool high)
{
    if (high) LATBSET = 1u << 9;
    else LATBCLR = 1u << 9;
}
static bool busy_level(void) { return (PORTB & (1u << 12)) != 0; }
static bool spi_start(bool data, const uint8_t *bytes, size_t count)
{
    if (spi_state != EINK_BUS_IDLE || !bytes || !count || count > sizeof spi_bytes)
        return false;
    memcpy(spi_bytes, bytes, count);
    spi_count = (uint8_t)count; spi_index = 0;
    if (data) LATBSET = 1u << 4; else LATBCLR = 1u << 4;
    LATACLR = 1u << 0;
    spi_started = _CP0_GET_COUNT();
    SPI1BUF = spi_bytes[0];
    spi_state = EINK_BUS_BUSY;
    return true;
}
static EInkBusState spi_poll(void)
{
    if (spi_state != EINK_BUS_BUSY) return spi_state;
    uint32_t age = (uint32_t)(_CP0_GET_COUNT() - spi_started);
    if (SPI1STATbits.SPIROV || age >= 100000u) {
        SPI1CONCLR = _SPI1CON_ON_MASK;
        LATASET = 1u << 0;
        return spi_state = EINK_BUS_FAULT;
    }
    if (age < 20u || !SPI1STATbits.SPITBE || SPI1STATbits.SPIBUSY) return spi_state;
    if (SPI1STATbits.SPIRBF) (void)SPI1BUF;
    if (++spi_index == spi_count) {
        LATASET = 1u << 0;
        return spi_state = EINK_BUS_IDLE;
    }
    spi_started = _CP0_GET_COUNT();
    SPI1BUF = spi_bytes[spi_index];
    return spi_state;
}
static const EInkAsyncPort eink_port = {
    spi_start, spi_poll, reset_pin, busy_level, Pic1Board_Now
};
const EInkAsyncPort *Pic1Board_EInkPort(void) { return &eink_port; }

bool Pic1Board_Init(void)
{
    last_tick = _CP0_GET_COUNT(); milliseconds = 0;
    motor_enabled = fault = false; initialized = true;
    last_adc = last_motion_post = 0;
    last_x = last_y = last_activity_x = last_activity_y = 512;
    SpoonServo_Init(); /* Only RA4/OC4/Timer2; output stays LOW. */
    JoystickADC_Init();
    if (!Follower_Init(&follower, &follower_config, 0)) return false;
    /* E-ink owns SPI1. Never touch UART1, UART2 or the tested SPI2 pins here. */
    uint32_t saved = __builtin_disable_interrupts();
    SYSKEY = 0; SYSKEY = 0xaa996655; SYSKEY = 0x556699aa;
    CFGCONbits.IOLOCK = 0;
    RPA0R = 0; RPA1R = 3; RPB4R = 0; RPB9R = 0; RPB12R = 0;
    RPB5R = 0; /* Reserved treat pin14 stays LOW, never starts OC2. */
    CFGCONbits.IOLOCK = 1; SYSKEY = 0;
    PMD5bits.SPI1MD = 0;
    SPI1CON = 0; SPI1CON2 = 0;
    ANSELACLR = 3u;
    ANSELBCLR = (1u<<4)|(1u<<5)|(1u<<9)|(1u<<12)|(1u<<14);
    ODCACLR = 3u;
    ODCBCLR = (1u<<4)|(1u<<5)|(1u<<9)|(1u<<14);
    LATASET = 1u; LATACLR = 2u;
    LATBSET = 1u<<9; LATBCLR = (1u<<4)|(1u<<5)|(1u<<14);
    TRISACLR = 3u;
    TRISBCLR = (1u<<4)|(1u<<5)|(1u<<9)|(1u<<14);
    TRISBSET = 1u<<12;
    CNPUBCLR = 1u<<12; CNPDBSET = 1u<<12;
    SPI1BRG = 99; /* Same 100 kHz mode 0 as tested EInkBench. */
    (void)SPI1BUF; SPI1STATCLR = _SPI1STAT_SPIROV_MASK;
    SPI1CON = _SPI1CON_MSTEN_MASK|_SPI1CON_CKE_MASK|_SPI1CON_DISSDI_MASK|_SPI1CON_ON_MASK;
    spi_state = EINK_BUS_IDLE;
    if (saved & 1u) __builtin_enable_interrupts();
    return true;
}

bool Pic1Board_Motor(uint32_t angle_mdeg, bool home, bool enabled)
{
    /* Follower owns the calibrated pulse target; angle is only gameplay input.
     * X checkpoint intentionally supplies home=true to suppress angular score.
     */
    (void)angle_mdeg; (void)home;
    motor_enabled = enabled;
    if (!enabled) SpoonServo_Disable();
    return !fault;
}

void Pic1Board_Stop(void)
{
    motor_enabled = false;
    SpoonServo_Disable();
    LATBCLR = 1u<<5;
}

bool Pic1Board_Service(void)
{
    if (!initialized || fault) return false;
    uint32_t now = Pic1Board_Now();
    uint16_t x, y;
    JoystickADC_State adc = JoystickADC_Poll(&x, &y);
    if (adc == JOYSTICK_ADC_READY) {
        last_x = x; last_y = y;
        Follower_Sample(&follower, x, y, now);
    } else if (adc == JOYSTICK_ADC_FAULT) Follower_Fault(&follower);
    if ((uint32_t)(now-last_adc) >= 10u && JoystickADC_Start()) last_adc = now;
    Follower_State state = Follower_Service(&follower, now);
    if (state == FOLLOWER_FAULT) { fault = true; Pic1Board_Stop(); return false; }
    if (state == FOLLOWER_ACTIVE && adc == JOYSTICK_ADC_READY &&
        (uint32_t)(now-last_motion_post) >= 50u) {
        last_motion_post = now;
        int32_t dx = (int32_t)last_x-last_activity_x;
        int32_t dy = (int32_t)last_y-last_activity_y;
        /* Reuse the checkpoint deadband as an activity/noise band. Y does not
         * gate startup or activity in X mode. No held-stick auto activity. */
        bool moved = dx > 40 || dx < -40;
#if PIC1_JOYSTICK_MODE == 2
        moved = moved || dy > 40 || dy < -40;
        float angle = follower.reference_angle_deg;
        if (angle < 0) angle += 360.0f;
        uint32_t mdeg = (uint32_t)(angle*1000.0f) % 360000u;
        bool home = !follower.tracking;
#else
        (void)dy;
        uint32_t mdeg = 0;
        bool home = true; /* No claim that X-only motion measures rotation. */
#endif
        if (moved) { last_activity_x = last_x; last_activity_y = last_y; }
        if (!GameIO_Joystick(mdeg, home, moved, now)) { fault = true; return false; }
    }
    if (PIC1_ENABLE_SPOON && motor_enabled && state == FOLLOWER_ACTIVE) {
        if (!SpoonServo_WritePulse(follower.pulse_us)) { fault = true; return false; }
    } else SpoonServo_Disable();
    return true;
}
