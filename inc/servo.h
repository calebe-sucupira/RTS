#pragma once

#include <stdint.h>

typedef struct
{
    uint32_t pin;
    uint32_t slice;
    uint32_t channel;
} servo_t;

void servo_init(
    servo_t *servo,
    uint32_t pin
);

void servo_set_angle(
    const servo_t *servo,
    uint8_t angle
);
