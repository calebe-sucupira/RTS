#pragma once

#include <stdbool.h>

#include "servo.h"
#include "ultrasonic.h"

bool app_tasks_start(
    servo_t *servo,
    ultrasonic_t *ultrasonic
);
