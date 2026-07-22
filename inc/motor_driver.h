#pragma once

typedef enum
{
    MOTOR_STOP_COAST,
    MOTOR_STOP_BRAKE,
} motor_stop_mode_t;

void motor_driver_init(void);

void motor_driver_set(
    float left_speed,
    float right_speed
);

void motor_driver_forward(float speed);
void motor_driver_reverse(float speed);
void motor_driver_turn_left(float speed);
void motor_driver_turn_right(float speed);

void motor_driver_stop(motor_stop_mode_t mode);
