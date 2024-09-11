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

// Estruturas globais e variáveis
servo_t myServo;
uint8_t current_servo_angle;
SemaphoreHandle_t sync_servo_sensor;

float distances[45];

#define QUEUE_SIZE 45
QueueHandle_t distanceQueue;

// Função para configurar os pinos do sensor
void setup_sensor() {
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
}

// Tarefa responsável por mover o servo motor em uma varredura de 180 graus
void servo_task() {
    float distance_cm;
    uint8_t angle = 0;
    int8_t step = 4;  // Controla o sentido de movimento do servo

    while (1) {
        printf("Movendo servo para o ângulo: %d\n", angle);

        // Mover o servo
        servoWrite(&myServo, angle);

        // Liberar o semáforo para o sensor realizar a leitura
        xSemaphoreGive(sync_servo_sensor);
        vTaskDelay(pdMS_TO_TICKS(25));
        xSemaphoreTake(sync_servo_sensor, 10);

        if (xQueueReceive(distanceQueue, &distance_cm, 0)) {
            distances[angle/4] = distance_cm;
        }
        printf("Ditancia %.2f\n", distances[angle/4]);

        // Atualizar o ângulo do servo
        angle += step;

        // Inverter o sentido quando atingir os limites de 0 ou 180 graus
        if (angle == 0 || angle == 180) {
            step = -step;
        }
    }
}

// Tarefa responsável por coletar a distância medida pelo sensor ultrassônico
void sensor_task() {
    uint32_t start_time, end_time, pulse_duration;
    float distance_cm;

    while (1) {
        // Esperar a permissão da tarefa do servo
        if (xSemaphoreTake(sync_servo_sensor, 10) == pdTRUE) {
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

            xQueueSend(distanceQueue, &distance_cm, 0);
            //printf("Tempo de Resposta %u\n", pulse_duration);
            //printf("Ditancia %.2f\n", distance_cm);
            // Liberar o semáforo para o servo continuar
            xSemaphoreGive(sync_servo_sensor);
        }
    } 
}
// Tarefa responsável por controlar o carriho
void decision_task() {

}
int main() {
    stdio_init_all();
    setup_sensor();
    servoAttach(&myServo, SERVO_PIN);

    // Criação dos semáforos
    sync_servo_sensor = xSemaphoreCreateBinary();

    // Criação das filas
    distanceQueue = xQueueCreate(QUEUE_SIZE, sizeof(float));
    if (distanceQueue == NULL) {
        printf("Erro ao criar a fila.\n");
        return -1;
    }

    // Inicializar o semáforo para começar a sincronização corretamente
    xSemaphoreGive(sync_servo_sensor);

    // Criação das tarefas
    xTaskCreate(servo_task, "Servo Task", 256, NULL, 1, NULL);
    xTaskCreate(sensor_task, "Sensor Task", 256, NULL, 1, NULL);
    //xTaskCreate(decision_task, "Decision Task", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1) {}

    return 0;
}
