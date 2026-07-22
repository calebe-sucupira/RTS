#include "motor_driver.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "robot_config.h"

typedef struct
{
    uint input_1_pin;
    uint input_2_pin;
    uint pwm_pin;
    uint slice;
    uint channel;
    bool inverted;
} motor_channel_t;

static motor_channel_t s_left_motor;
static motor_channel_t s_right_motor;

static float clamp_speed(float speed)
{
    if (speed > 1.0f)
    {
        return 1.0f;
    }

    if (speed < -1.0f)
    {
        return -1.0f;
    }

    return speed;
}

static void configure_output_pin(uint pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);
}

static void configure_motor(
    motor_channel_t *motor,
    uint input_1_pin,
    uint input_2_pin,
    uint pwm_pin,
    bool inverted
)
{
    configure_output_pin(input_1_pin);
    configure_output_pin(input_2_pin);

    gpio_set_function(pwm_pin, GPIO_FUNC_PWM);

    motor->input_1_pin = input_1_pin;
    motor->input_2_pin = input_2_pin;
    motor->pwm_pin = pwm_pin;
    motor->slice = pwm_gpio_to_slice_num(pwm_pin);
    motor->channel = pwm_gpio_to_channel(pwm_pin);
    motor->inverted = inverted;

    float divider =
        (float)clock_get_hz(clk_sys) /
        (
            ROBOT_MOTOR_PWM_FREQUENCY_HZ *
            (ROBOT_MOTOR_PWM_WRAP + 1)
        );

    pwm_set_clkdiv(motor->slice, divider);
    pwm_set_wrap(
        motor->slice,
        ROBOT_MOTOR_PWM_WRAP
    );

    pwm_set_chan_level(
        motor->slice,
        motor->channel,
        0
    );

    pwm_set_enabled(motor->slice, true);
}

static void apply_motor(
    const motor_channel_t *motor,
    float speed
)
{
    speed = clamp_speed(speed);

    if (motor->inverted)
    {
        speed = -speed;
    }

    uint16_t pwm_level =
        (uint16_t)(
            fabsf(speed) *
            ROBOT_MOTOR_PWM_WRAP
        );

    if (speed > 0.01f)
    {
        gpio_put(motor->input_1_pin, 1);
        gpio_put(motor->input_2_pin, 0);
    }
    else if (speed < -0.01f)
    {
        gpio_put(motor->input_1_pin, 0);
        gpio_put(motor->input_2_pin, 1);
    }
    else
    {
        gpio_put(motor->input_1_pin, 0);
        gpio_put(motor->input_2_pin, 0);
        pwm_level = 0;
    }

    pwm_set_chan_level(
        motor->slice,
        motor->channel,
        pwm_level
    );
}

static void apply_stop(
    const motor_channel_t *motor,
    motor_stop_mode_t mode
)
{
    if (mode == MOTOR_STOP_BRAKE)
    {
        gpio_put(motor->input_1_pin, 1);
        gpio_put(motor->input_2_pin, 1);

        pwm_set_chan_level(
            motor->slice,
            motor->channel,
            ROBOT_MOTOR_PWM_WRAP
        );

        return;
    }

    gpio_put(motor->input_1_pin, 0);
    gpio_put(motor->input_2_pin, 0);

    pwm_set_chan_level(
        motor->slice,
        motor->channel,
        0
    );
}

void motor_driver_init(void)
{
    configure_motor(
        &s_left_motor,
        ROBOT_LEFT_MOTOR_IN1_PIN,
        ROBOT_LEFT_MOTOR_IN2_PIN,
        ROBOT_LEFT_MOTOR_PWM_PIN,
        ROBOT_LEFT_MOTOR_INVERTED
    );

    configure_motor(
        &s_right_motor,
        ROBOT_RIGHT_MOTOR_IN1_PIN,
        ROBOT_RIGHT_MOTOR_IN2_PIN,
        ROBOT_RIGHT_MOTOR_PWM_PIN,
        ROBOT_RIGHT_MOTOR_INVERTED
    );

    motor_driver_stop(MOTOR_STOP_COAST);
}

void motor_driver_set(
    float left_speed,
    float right_speed
)
{
    apply_motor(&s_left_motor, left_speed);
    apply_motor(&s_right_motor, right_speed);
}

void motor_driver_forward(float speed)
{
    speed = fabsf(speed);
    motor_driver_set(speed, speed);
}

void motor_driver_reverse(float speed)
{
    speed = fabsf(speed);
    motor_driver_set(-speed, -speed);
}

void motor_driver_turn_left(float speed)
{
    speed = fabsf(speed);
    motor_driver_set(-speed, speed);
}

void motor_driver_turn_right(float speed)
{
    speed = fabsf(speed);
    motor_driver_set(speed, -speed);
}

void motor_driver_stop(motor_stop_mode_t mode)
{
    apply_stop(&s_left_motor, mode);
    apply_stop(&s_right_motor, mode);
}
