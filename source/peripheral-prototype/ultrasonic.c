#include "board.h"
#include "ultrasonic.h"

#define ECHO_TIMEOUT_US 30000u
#define SAMPLE_INTERVAL_TICKS (100000u * CORE_TICKS_PER_US)

static uint32_t trigger_mask;
static uint32_t echo_mask;
static uint32_t last_sample_ticks;

typedef enum {
    RANGE_OK,
    ECHO_STUCK_HIGH,
    ECHO_NO_RISE,
    ECHO_NO_FALL,
    RANGE_OUTSIDE_LIMITS
} RangeStatus;

volatile RangeStatus range_status;
volatile uint32_t echo_us;
volatile uint32_t distance_mm;

static void uart_start(bool shared_board)
{
    pps_unlock();
    if (shared_board) {
        LATBSET = 1u << 4;
        TRISBCLR = 1u << 4;
        RPB4R = 1u; /* U1TX, DIP 11 */
    } else {
        ANSELACLR = 1u;
        LATASET = 1u;
        TRISACLR = 1u;
        RPA0R = 1u; /* U1TX, DIP 2 */
    }
    pps_lock();

    U1MODE = 0;
    U1STA = 0;
    U1BRG = 129u; /* 20 MHz / (16 * 130), approximately 9600 baud */
    U1MODEbits.ON = 1;
    U1STAbits.UTXEN = 1;
}

static void put_char(char c)
{
    while (U1STAbits.UTXBF) {
    }
    U1TXREG = (unsigned char)c;
}

void ultrasonic_write_text(const char *s)
{
    while (*s != '\0') {
        put_char(*s++);
    }
}

static void put_uint(uint32_t n)
{
    char digits[10];
    unsigned count = 0;
    do {
        digits[count++] = (char)('0' + n % 10u);
        n /= 10u;
    } while (n != 0u);
    while (count != 0u) {
        put_char(digits[--count]);
    }
}

static bool wait_for_echo(bool high, uint32_t *edge)
{
    const uint32_t start = ticks_now();
    while (((PORTB & echo_mask) != 0u) != high) {
        if ((uint32_t)(ticks_now() - start) >= ECHO_TIMEOUT_US * CORE_TICKS_PER_US) {
            return false;
        }
    }
    *edge = ticks_now();
    return true;
}

static RangeStatus measure(uint32_t *pulse_us)
{
    uint32_t rise;
    uint32_t fall;
    if ((PORTB & echo_mask) != 0u) {
        return ECHO_STUCK_HIGH;
    }

    LATBSET = trigger_mask;
    delay_us(12u);
    LATBCLR = trigger_mask;
    if (!wait_for_echo(true, &rise)) {
        return ECHO_NO_RISE;
    }
    if (!wait_for_echo(false, &fall)) {
        return ECHO_NO_FALL;
    }

    *pulse_us = ((uint32_t)(fall - rise) + CORE_TICKS_PER_US / 2u)
                / CORE_TICKS_PER_US;
    if (*pulse_us < 116u || *pulse_us > 23200u) {
        return RANGE_OUTSIDE_LIMITS;
    }
    return RANGE_OK;
}

void ultrasonic_start(bool shared_board)
{
    trigger_mask = 1u << (shared_board ? 2u : 7u); /* DIP 6 or 16 */
    echo_mask = 1u << (shared_board ? 3u : 8u);    /* DIP 7 or 17 */
    if (shared_board) {
        ANSELBCLR = trigger_mask | echo_mask;
    }
    LATBCLR = trigger_mask;
    TRISBCLR = trigger_mask;
    TRISBSET = echo_mask;
    uart_start(shared_board);
    ultrasonic_write_text("HC-SR04 ready\r\n");
    last_sample_ticks = ticks_now();
}

bool ultrasonic_poll(void)
{
    static const char *const errors[] = {
        "", "echo already high", "no rising edge", "no falling edge", "out of range"
    };
    if ((uint32_t)(ticks_now() - last_sample_ticks) < SAMPLE_INTERVAL_TICKS) {
        return false;
    }

    uint32_t pulse = 0;
    const RangeStatus status = measure(&pulse);
    echo_us = pulse;
    distance_mm = status == RANGE_OK ? (pulse * 10u + 29u) / 58u : 0u;
    range_status = status;
    if (status == RANGE_OK) {
        put_uint(distance_mm);
        ultrasonic_write_text(" mm, echo ");
        put_uint(pulse);
        ultrasonic_write_text(" us\r\n");
    } else {
        ultrasonic_write_text(errors[status]);
        ultrasonic_write_text("\r\n");
    }
    last_sample_ticks = ticks_now();
    return true;
}

uint32_t ultrasonic_distance_mm(void)
{
    return distance_mm;
}
