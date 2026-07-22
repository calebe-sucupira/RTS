#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t trigger_pin;
    uint32_t echo_pin;
    uint32_t timeout_us;
} ultrasonic_t;

void ultrasonic_init(
    ultrasonic_t *sensor,
    uint32_t trigger_pin,
    uint32_t echo_pin,
    uint32_t timeout_us
);

bool ultrasonic_measure_cm(
    const ultrasonic_t *sensor,
    float *distance_cm
);
