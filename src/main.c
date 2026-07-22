#include <stdio.h>

#include "FreeRTOS.h"
#include "app_tasks.h"
#include "motor_driver.h"
#include "pico/stdlib.h"
#include "robot_config.h"
#include "servo.h"
#include "task.h"
#include "ultrasonic.h"

void vApplicationMallocFailedHook(void)
{
    motor_driver_stop(MOTOR_STOP_BRAKE);
    taskDISABLE_INTERRUPTS();

    while (true)
    {
        tight_loop_contents();
    }
}

void vApplicationStackOverflowHook(
    TaskHandle_t task,
    char *task_name
)
{
    (void)task;
    (void)task_name;

    motor_driver_stop(MOTOR_STOP_BRAKE);
    taskDISABLE_INTERRUPTS();

    while (true)
    {
        tight_loop_contents();
    }
}

int main(void)
{
    stdio_init_all();

    static servo_t servo;
    static ultrasonic_t ultrasonic;

    motor_driver_init();

    servo_init(
        &servo,
        ROBOT_SERVO_PIN
    );

    servo_set_angle(
        &servo,
        ROBOT_SERVO_CENTER_ANGLE
    );

    ultrasonic_init(
        &ultrasonic,
        ROBOT_ULTRASONIC_TRIGGER_PIN,
        ROBOT_ULTRASONIC_ECHO_PIN,
        ROBOT_ULTRASONIC_TIMEOUT_US
    );

    if (!app_tasks_start(&servo, &ultrasonic))
    {
        printf(
            "Falha ao inicializar recursos do FreeRTOS.\n"
        );

        motor_driver_stop(MOTOR_STOP_BRAKE);
        return 1;
    }

    printf("Carrinho autônomo inicializado.\n");

    vTaskStartScheduler();

    motor_driver_stop(MOTOR_STOP_BRAKE);

    while (true)
    {
        tight_loop_contents();
    }
}
