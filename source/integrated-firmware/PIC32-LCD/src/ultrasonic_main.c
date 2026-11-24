#include "ultrasonic.h"

int main(void)
{
    ultrasonic_start(false);
    for (;;) {
        ultrasonic_poll();
    }
}
