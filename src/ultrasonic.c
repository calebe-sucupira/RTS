#include "ultrasonic.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#define SPEED_OF_SOUND_CM_PER_US 0.0343f
#define MINIMUM_DISTANCE_CM 2.0f
#define MAXIMUM_DISTANCE_CM 400.0f

static bool wait_for_level(
    uint32_t pin,
    bool level,
    uint32_t timeout_us
)
{
    uint32_t start_time = time_us_32();

    while (gpio_get(pin) != level)
    {
        if (
            (uint32_t)(time_us_32() - start_time) >=
            timeout_us
        )
        {
            return false;
        }

        tight_loop_contents();
    }

    return true;
}

void ultrasonic_init(
    ultrasonic_t *sensor,
    uint32_t trigger_pin,
    uint32_t echo_pin,
    uint32_t timeout_us
)
{
    sensor->trigger_pin = trigger_pin;
    sensor->echo_pin = echo_pin;
    sensor->timeout_us = timeout_us;

    gpio_init(trigger_pin);
    gpio_set_dir(trigger_pin, GPIO_OUT);
    gpio_put(trigger_pin, 0);

    gpio_init(echo_pin);
    gpio_set_dir(echo_pin, GPIO_IN);
}

bool ultrasonic_measure_cm(
    const ultrasonic_t *sensor,
    float *distance_cm
)
{
    if (distance_cm == NULL)
    {
        return false;
    }

    if (
        !wait_for_level(
            sensor->echo_pin,
            false,
            sensor->timeout_us
        )
    )
    {
        return false;
    }

    gpio_put(sensor->trigger_pin, 0);
    sleep_us(2);

    gpio_put(sensor->trigger_pin, 1);
    sleep_us(10);

    gpio_put(sensor->trigger_pin, 0);

    if (
        !wait_for_level(
            sensor->echo_pin,
            true,
            sensor->timeout_us
        )
    )
    {
        return false;
    }

    uint32_t pulse_start = time_us_32();

    if (
        !wait_for_level(
            sensor->echo_pin,
            false,
            sensor->timeout_us
        )
    )
    {
        return false;
    }

    uint32_t pulse_duration =
        time_us_32() - pulse_start;

    float measured_distance =
        pulse_duration *
        SPEED_OF_SOUND_CM_PER_US /
        2.0f;

    if (
        measured_distance < MINIMUM_DISTANCE_CM ||
        measured_distance > MAXIMUM_DISTANCE_CM
    )
    {
        return false;
    }

    *distance_cm = measured_distance;
    return true;
}
