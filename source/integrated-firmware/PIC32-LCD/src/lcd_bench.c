#include "display_spi.h"
#include "heat_audio.h"
#include "heat_lcd.h"
#include "st7789.h"

int main(void)
{
    st7789_prepare_pins();
    display_spi_start();
    heat_audio_start();
    if (!heat_lcd_start()) {
        for (;;) {
            /* Inspect the SPI signals if the controller could not start. */
        }
    }

    enum { NEAR_MM = 50, FAR_MM = 400, STEP_MM = 5 };
    int distance_mm = FAR_MM;
    int step_mm = -STEP_MM;
    uint32_t last_sample = ticks_now();
    for (;;) {
        const uint32_t now = ticks_now();
        if ((uint32_t)(now - last_sample) >= 60000u * CORE_TICKS_PER_US) {
            last_sample = now;
            heat_lcd_set_distance((uint32_t)distance_mm);
            heat_audio_update((uint32_t)distance_mm);

            if (distance_mm == NEAR_MM) {
                step_mm = STEP_MM;
            } else if (distance_mm == FAR_MM) {
                step_mm = -STEP_MM;
            }
            distance_mm += step_mm;
        }
        heat_lcd_update();
        delay_us(1000u);
    }
}
