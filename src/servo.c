#include "servo.h"

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

#define SERVO_PWM_FREQUENCY_HZ 50
#define SERVO_PWM_PERIOD_US 20000
#define SERVO_MIN_PULSE_US 500
#define SERVO_MAX_PULSE_US 2500

void servo_init(
    servo_t *servo,
    uint32_t pin
)
{
    servo->pin = pin;
    servo->slice = pwm_gpio_to_slice_num(pin);
    servo->channel = pwm_gpio_to_channel(pin);

    gpio_set_function(pin, GPIO_FUNC_PWM);

    float divider =
        (float)clock_get_hz(clk_sys) /
        1000000.0f;

    pwm_set_clkdiv(servo->slice, divider);

    pwm_set_wrap(
        servo->slice,
        SERVO_PWM_PERIOD_US - 1
    );

    pwm_set_enabled(servo->slice, true);
    servo_set_angle(servo, 0);
}

void servo_set_angle(
    const servo_t *servo,
    uint8_t angle
)
{
    if (angle > 180)
    {
        angle = 180;
    }

    uint32_t pulse_width =
        SERVO_MIN_PULSE_US +
        (
            (
                SERVO_MAX_PULSE_US -
                SERVO_MIN_PULSE_US
            ) *
            angle
        ) /
        180;

    pwm_set_chan_level(
        servo->slice,
        servo->channel,
        pulse_width
    );
}
