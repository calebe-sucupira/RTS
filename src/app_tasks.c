#include "app_tasks.h"

#include <float.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "motor_driver.h"
#include "queue.h"
#include "robot_config.h"
#include "semphr.h"
#include "task.h"

typedef enum
{
    NAVIGATION_TURN_LEFT,
    NAVIGATION_TURN_RIGHT,
} navigation_turn_t;

static QueueHandle_t s_distance_queue;
static QueueHandle_t s_turn_queue;
static SemaphoreHandle_t s_sensor_mutex;

static TaskHandle_t s_move_task;
static TaskHandle_t s_scan_task;
static TaskHandle_t s_decision_task;
static TaskHandle_t s_sensor_task;

static servo_t *s_servo;
static ultrasonic_t *s_ultrasonic;

static bool measure_distance(float *distance)
{
    if (
        xSemaphoreTake(
            s_sensor_mutex,
            pdMS_TO_TICKS(100)
        ) != pdTRUE
    )
    {
        return false;
    }

    bool result = ultrasonic_measure_cm(
        s_ultrasonic,
        distance
    );

    xSemaphoreGive(s_sensor_mutex);

    return result;
}

static void sensor_task(void *parameters)
{
    (void)parameters;

    TickType_t last_wake_time =
        xTaskGetTickCount();

    while (true)
    {
        float distance = 0.0f;

        if (!measure_distance(&distance))
        {
            distance = 0.0f;
        }

        xQueueOverwrite(
            s_distance_queue,
            &distance
        );

        vTaskDelayUntil(
            &last_wake_time,
            pdMS_TO_TICKS(
                ROBOT_DISTANCE_PERIOD_MS
            )
        );
    }
}

static void move_task(void *parameters)
{
    (void)parameters;

    while (true)
    {
        float distance = 0.0f;

        BaseType_t received = xQueueReceive(
            s_distance_queue,
            &distance,
            pdMS_TO_TICKS(
                ROBOT_DISTANCE_WAIT_MS
            )
        );

        if (
            received == pdTRUE &&
            distance > ROBOT_OBSTACLE_THRESHOLD_CM
        )
        {
            motor_driver_forward(
                ROBOT_CRUISE_SPEED
            );

            continue;
        }

        motor_driver_stop(MOTOR_STOP_BRAKE);

        if (received != pdTRUE)
        {
            printf(
                "Sem leitura do sensor; motores parados.\n"
            );
        }
        else
        {
            printf(
                "Obstáculo detectado a %.1f cm.\n",
                distance
            );
        }

        xTaskNotifyGive(s_scan_task);

        ulTaskNotifyTake(
            pdTRUE,
            portMAX_DELAY
        );
    }
}

static void scan_task(void *parameters)
{
    (void)parameters;

    while (true)
    {
        ulTaskNotifyTake(
            pdTRUE,
            portMAX_DELAY
        );

        motor_driver_stop(MOTOR_STOP_BRAKE);

        float left_clearance = FLT_MAX;
        float right_clearance = FLT_MAX;

        for (
            uint8_t angle = ROBOT_SCAN_MIN_ANGLE;
            angle <= ROBOT_SCAN_MAX_ANGLE;
            angle += ROBOT_SCAN_STEP_ANGLE
        )
        {
            servo_set_angle(s_servo, angle);

            vTaskDelay(
                pdMS_TO_TICKS(
                    ROBOT_SCAN_SERVO_SETTLE_MS
                )
            );

            float distance = 0.0f;

            if (!measure_distance(&distance))
            {
                distance = 0.0f;
            }

            if (
                angle <
                ROBOT_SERVO_CENTER_ANGLE
            )
            {
                if (distance < right_clearance)
                {
                    right_clearance = distance;
                }
            }
            else if (
                angle >
                ROBOT_SERVO_CENTER_ANGLE
            )
            {
                if (distance < left_clearance)
                {
                    left_clearance = distance;
                }
            }
        }

        navigation_turn_t turn =
            right_clearance >= left_clearance
                ? NAVIGATION_TURN_RIGHT
                : NAVIGATION_TURN_LEFT;

        printf(
            "Área livre: esquerda %.1f cm, direita %.1f cm.\n",
            left_clearance,
            right_clearance
        );

        xQueueOverwrite(s_turn_queue, &turn);
        xTaskNotifyGive(s_decision_task);
    }
}

static void decision_task(void *parameters)
{
    (void)parameters;

    while (true)
    {
        ulTaskNotifyTake(
            pdTRUE,
            portMAX_DELAY
        );

        navigation_turn_t turn;

        if (
            xQueueReceive(
                s_turn_queue,
                &turn,
                0
            ) != pdTRUE
        )
        {
            motor_driver_stop(MOTOR_STOP_BRAKE);
            continue;
        }

        if (turn == NAVIGATION_TURN_RIGHT)
        {
            printf("Virando à direita.\n");

            motor_driver_turn_right(
                ROBOT_TURN_SPEED
            );
        }
        else
        {
            printf("Virando à esquerda.\n");

            motor_driver_turn_left(
                ROBOT_TURN_SPEED
            );
        }

        vTaskDelay(
            pdMS_TO_TICKS(
                ROBOT_TURN_DURATION_MS
            )
        );

        motor_driver_stop(MOTOR_STOP_BRAKE);

        servo_set_angle(
            s_servo,
            ROBOT_SERVO_CENTER_ANGLE
        );

        xQueueReset(s_distance_queue);

        vTaskDelay(
            pdMS_TO_TICKS(
                ROBOT_POST_TURN_SETTLE_MS
            )
        );

        xTaskNotifyGive(s_move_task);
    }
}

static void delete_task(TaskHandle_t *task)
{
    if (*task != NULL)
    {
        vTaskDelete(*task);
        *task = NULL;
    }
}

static void cleanup_resources(void)
{
    delete_task(&s_move_task);
    delete_task(&s_scan_task);
    delete_task(&s_decision_task);
    delete_task(&s_sensor_task);

    if (s_distance_queue != NULL)
    {
        vQueueDelete(s_distance_queue);
        s_distance_queue = NULL;
    }

    if (s_turn_queue != NULL)
    {
        vQueueDelete(s_turn_queue);
        s_turn_queue = NULL;
    }

    if (s_sensor_mutex != NULL)
    {
        vSemaphoreDelete(s_sensor_mutex);
        s_sensor_mutex = NULL;
    }
}

bool app_tasks_start(
    servo_t *servo,
    ultrasonic_t *ultrasonic
)
{
    if (servo == NULL || ultrasonic == NULL)
    {
        return false;
    }

    s_servo = servo;
    s_ultrasonic = ultrasonic;

    s_distance_queue =
        xQueueCreate(1, sizeof(float));

    s_turn_queue =
        xQueueCreate(
            1,
            sizeof(navigation_turn_t)
        );

    s_sensor_mutex =
        xSemaphoreCreateMutex();

    if (
        s_distance_queue == NULL ||
        s_turn_queue == NULL ||
        s_sensor_mutex == NULL
    )
    {
        cleanup_resources();
        return false;
    }

    if (
        xTaskCreate(
            scan_task,
            "scan",
            ROBOT_TASK_STACK_SIZE,
            NULL,
            ROBOT_SCAN_TASK_PRIORITY,
            &s_scan_task
        ) != pdPASS
    )
    {
        cleanup_resources();
        return false;
    }

    if (
        xTaskCreate(
            decision_task,
            "decision",
            ROBOT_TASK_STACK_SIZE,
            NULL,
            ROBOT_DECISION_TASK_PRIORITY,
            &s_decision_task
        ) != pdPASS
    )
    {
        cleanup_resources();
        return false;
    }

    if (
        xTaskCreate(
            sensor_task,
            "distance",
            ROBOT_TASK_STACK_SIZE,
            NULL,
            ROBOT_SENSOR_TASK_PRIORITY,
            &s_sensor_task
        ) != pdPASS
    )
    {
        cleanup_resources();
        return false;
    }

    if (
        xTaskCreate(
            move_task,
            "movement",
            ROBOT_TASK_STACK_SIZE,
            NULL,
            ROBOT_MOVE_TASK_PRIORITY,
            &s_move_task
        ) != pdPASS
    )
    {
        cleanup_resources();
        return false;
    }

    return true;
}
