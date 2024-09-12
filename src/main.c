#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>
#include "servo.h"
#include "queue.h"

#define SERVO_PIN 12
#define TRIG_PIN 10
#define ECHO_PIN 11
#define PWM_FREQUENCY 1000
#define PIN_MOTOR_LEFT  2 
#define PIN_MOTOR_RIGHT 3  

// Estruturas globais e variáveis
servo_t myServo;
uint8_t current_servo_angle;
uint slice_num_left;
uint slice_num_right;

#define QUEUE_SIZE 180
QueueHandle_t distanceQueue;

// Handles para as tarefas
TaskHandle_t servoTaskHandle = NULL;
TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t decisionTaskHandle = NULL;


// Função para configurar os pinos do sensor
void setup_sensor() {
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);

    gpio_init(PIN_MOTOR_LEFT);
    gpio_set_dir(PIN_MOTOR_LEFT, GPIO_OUT);
    gpio_init(PIN_MOTOR_RIGHT);
    gpio_set_dir(PIN_MOTOR_RIGHT, GPIO_OUT);
}


// Tarefa responsável por mover o servo motor em uma varredura de 180 graus
void vTaskServo() {
    uint8_t angle = 0;
    int8_t step = 4;  // Controla o sentido de movimento do servo

    while (1) {
        printf("Movendo servo para o ângulo: %d\n", angle);
        current_servo_angle = angle;
        // Mover o servo
        servoWrite(&myServo, angle);

        // Notificar a tarefa de sensor para realizar a leitura
        xTaskNotifyGive(sensorTaskHandle);
        vTaskDelay(pdMS_TO_TICKS(25));

        // Atualizar o ângulo do servo
        angle += step;

        // Inverter o sentido quando atingir os limites de 0 ou 180 graus
        if (angle >= 180 || angle <= 0) {
            step = -step;
        }

        // Aguardar a notificação para sincronizar com a tarefa do sensor
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

// Tarefa responsável por coletar a distância medida pelo sensor ultrassônico
void vTaskSensor() {
    uint32_t start_time, end_time, pulse_duration;
    float distance_cm;

    while (1) {
        // Esperar a notificação da tarefa de servo
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Disparo do pulso
        gpio_put(TRIG_PIN, 0);
        sleep_us(2);
        gpio_put(TRIG_PIN, 1);
        sleep_us(10);
        gpio_put(TRIG_PIN, 0);

        // Esperar o início do pulso no ECHO
        while (gpio_get(ECHO_PIN) == 0);
        start_time = time_us_32();

        // Esperar o final do pulso no ECHO
        while (gpio_get(ECHO_PIN) == 1);
        end_time = time_us_32();

        // Calcular a duração do pulso e a distância
        pulse_duration = end_time - start_time;
        distance_cm = (pulse_duration * 0.034) / 2;

        // Enviar o valor da distância para a fila
        if (xQueueSend(distanceQueue, &distance_cm, 10) != pdTRUE) {
            printf("Fila cheia. Não foi possível enviar a distância.\n");
        }

        // Notificar a tarefa de decisão
        xTaskNotifyGive(decisionTaskHandle);
    }
}

// Tarefa responsável por controlar o carrinho
void vTaskDecision() {
    float distance_cm;
    bool obstacle = false;
    bool obstacle_right = false;
    bool obstacle_left = false;

    while (1) {
        // Esperar a notificação da tarefa do sensor
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Receber o valor da distância da fila
        if (xQueueReceive(distanceQueue, &distance_cm, 10) == pdTRUE) {
            printf("Distância: %.2f cm\n", distance_cm);

            // Verifica se o obstáculo está à frente (ângulo entre 88º e 92º)
            if (current_servo_angle >= 88 && current_servo_angle <= 92) {
                obstacle = distance_cm <= 50.0;
            }

             // Se houver um obstáculo à frente, verificar lados
            if (obstacle) {
                obstacle_right = (current_servo_angle < 88 && distance_cm <= 50.0);
                obstacle_left  = (current_servo_angle > 92 && distance_cm <= 50.0);
            } else {
                obstacle_right = false;
                obstacle_left  = false;
            }

            // Tomar decisões com base nos obstáculos
            if (obstacle_right && obstacle_left) {
                printf("Pare\n");
                gpio_put(PIN_MOTOR_LEFT, 0);
                gpio_put(PIN_MOTOR_RIGHT, 0);
            } else if (obstacle_right) {
                printf("Vire à esquerda\n");
                gpio_put(PIN_MOTOR_LEFT, 0);
                gpio_put(PIN_MOTOR_RIGHT, true);
            } else if (obstacle_left) {
                printf("Vire à direita\n");
                gpio_put(PIN_MOTOR_LEFT, true);
                gpio_put(PIN_MOTOR_RIGHT, 0);
            } else {
                printf("Continue\n");
               gpio_put(PIN_MOTOR_LEFT, true);
               gpio_put(PIN_MOTOR_RIGHT, true);
            }
        } else {
            printf("Erro ao receber da fila\n");
        }

        // Sincronizar com a próxima execução
        xTaskNotifyGive(servoTaskHandle);
    }
}

int main() {
    stdio_init_all();
    setup_sensor();
    servoAttach(&myServo, SERVO_PIN);
    

    // Criação da fila
    distanceQueue = xQueueCreate(QUEUE_SIZE, sizeof(float));
    if (distanceQueue == NULL) {
        printf("Erro ao criar a fila.\n");
        return -1;
    }

    // Criação das tarefas
    xTaskCreate(vTaskServo, "Servo Task", 256, NULL, 1, &servoTaskHandle);
    xTaskCreate(vTaskSensor, "Sensor Task", 256, NULL, 1, &sensorTaskHandle);
    xTaskCreate(vTaskDecision, "Decision Task", 256, NULL, 1, &decisionTaskHandle);

    gpio_put(PIN_MOTOR_LEFT, true);
    gpio_put(PIN_MOTOR_RIGHT, true);
    vTaskStartScheduler();

    while (1) {}

    return 0;
}