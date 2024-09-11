
#ifndef _SERVO_H_
#define _SERVO_H_

/* Standard C includes */
#include <stdint.h>

/* Macro definitions */
#define PWM_FREQ_HZ                         50


/**
 * @brief Structure to hold Servo motor parameters
 * 
 */
typedef struct
{
    uint8_t pin;
    uint8_t sliceNum;
    uint8_t channelNum;
    uint32_t wrapPoint;
}servo_t;


/**
 * @brief Function to initialize servo GPIO with 50Hz PWM frequency
 * and 0 deg angle
 * 
 * @param servo Object of servo_t structure
 * @param pin Pin at which servo PWM wire is connected to Pico
 */
void servoAttach(servo_t *servo, uint8_t pin);


/**
 * @brief Function to move the servo to the specified angle
 * 
 * @param servo Object of servo_t structure
 * @param angle Angle at which servo is to be moved (0 to 180 deg)
 */
void servoWrite(servo_t *servo, uint8_t angle);

#endif