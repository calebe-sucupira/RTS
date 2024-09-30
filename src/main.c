#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include <stdio.h>
#include "servo.h"

// Definições dos pinos
#define SERVO_PIN 16
#define TRIG_PIN 10
#define ECHO_PIN 11
#define PIN_MOTOR_LEFT  2
#define PIN_MOTOR_RIGHT 3

#define QUEUE_SIZE 180
QueueHandle_t distanceQueue;

// Declarações das funções
void setup_sensor(void);
void move_servo(uint8_t angle);
void check_for_obstacle(void);

servo_t myServo;

// Variáveis Globais
TaskHandle_t moveTaskHandle = NULL;
TaskHandle_t scanTaskHandle = NULL;
TaskHandle_t decisionTaskHandle = NULL;
TaskHandle_t measureTaskHandle = NULL;

uint8_t current_servo_angle = 0;
bool obstacle_detected = false;
float last_distance = 100.0;  // Variável global para armazenar a última distância lida

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

// Função para verificar se há um obstáculo
void check_for_obstacle() {
    if (last_distance < 20.0) {  // Supondo que um obstáculo está a menos de 30 cm
        obstacle_detected = true;
    } else {
        obstacle_detected = false;
    }
}

// Nova tarefa para medir a distância
void vTaskMeasureDistance(void *pvParameters) {
    uint32_t start_time, end_time, pulse_duration;
    while (1) {
        printf("Verificando se tem obstaculo\n");
        // Enviar pulso de trigger
        gpio_put(TRIG_PIN, 0);
        sleep_us(2);
        gpio_put(TRIG_PIN, 1);
        sleep_us(10);
        gpio_put(TRIG_PIN, 0);

       // Espera o pino ECHO subir (início do pulso de resposta)
        uint32_t timeout = 20000;  // Timeout de segurança
        while (gpio_get(ECHO_PIN) == 0 && timeout > 0) {
            timeout--;
            sleep_us(1);
        }

        if (timeout == 0) {
            printf("Erro: Timeout ao esperar o início do pulso de ECHO\n");
            last_distance = 400.0;  // Marca como falha
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;  // Pula para a próxima iteração sem tentar medir
        }

        // Registrar o tempo de início
        start_time = time_us_32();

        // Espera o pino ECHO cair (fim do pulso de resposta)
        timeout = 20000;  // Timeout ajustado para o tempo máximo de resposta
        while (gpio_get(ECHO_PIN) == 1 && timeout > 0) {
            timeout--;
            sleep_us(1);
        }

        if (timeout == 0) {
            printf("Erro: Timeout ao esperar o fim do pulso de ECHO\n");
            last_distance = 400.0;  // Marca como falha
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        // Registrar o tempo de fim
        end_time = time_us_32();

        // Calcula a duração do pulso
        pulse_duration = end_time - start_time;

        // Verifica se o valor faz sentido antes de calcular a distância
        if (pulse_duration > 0) {
            last_distance = (pulse_duration * 0.034) / 2;  // Distância em cm
            printf("Distância medida: %.2f cm\n", last_distance);
        } else {
            printf("Erro: Duração do pulso inválida\n");
            last_distance = 400;
        }
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}

// Tarefa para mover o carrinho para frente
void vTaskMove(void *pvParameters) {
    while (1) {
        // Verificar se há um obstáculo
        servoWrite(&myServo, 90);
        check_for_obstacle();

        // Movimentar o carrinho para frente
        gpio_put(PIN_MOTOR_LEFT, 1);
        gpio_put(PIN_MOTOR_RIGHT, 1);

        // Checar se obstáculo foi detectado
        if (obstacle_detected) {
            // Parar o carrinho
            gpio_put(PIN_MOTOR_LEFT, 0);
            gpio_put(PIN_MOTOR_RIGHT, 0);
            printf("Obstáculo detectado, parando o carrinho...\n");

            // Notificar a tarefa de varredura
            xTaskNotifyGive(scanTaskHandle);

            // Suspender a tarefa de movimentação até a decisão ser tomada
            vTaskSuspend(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}

// Tarefa para realizar a varredura com o servo motor
void vTaskScan(void *pvParameters) {
    uint8_t angle = 0;
    float distance;

    while (1) {
        if(!obstacle_detected){
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }

        servoWrite(&myServo, angle);
        vTaskDelay(pdMS_TO_TICKS(25));
        distance = last_distance;  // Usar a distância lida pela tarefa de medição
        angle++;
        if (xQueueSend(distanceQueue, &distance, pdMS_TO_TICKS(10)) != pdTRUE) {
            printf("Erro ao enviar distância para a fila.\n");
        }
        if(angle == 180){
            angle = 0;
            xTaskNotifyGive(decisionTaskHandle);
        }
    }
}

// Tarefa para tomar a decisão sobre o caminho a seguir
void vTaskDecision(void *pvParameters) {
    float distance;
    float left_distance = 9999, right_distance = 9999;
    uint8_t scan_count = 0;

    while (1) {

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);


        // Processar as distâncias da fila
        while (xQueueReceive(distanceQueue, &distance, 0) == pdTRUE) {
            if (right_distance > distance && scan_count <= 90) {
                right_distance = distance;
            } else if(left_distance > distance){
                left_distance = distance;  // Somar as distâncias da esquerda (90-180 graus)
            }
            scan_count++;
        }
        // Tomar decisão
        if (right_distance > left_distance) {
            printf("Virar à direita.\n");
            // Lógica para virar à direita
            gpio_put(PIN_MOTOR_LEFT, 1);
            gpio_put(PIN_MOTOR_RIGHT, 0);
            vTaskDelay(pdMS_TO_TICKS(500));
        } else {
            printf("Virar à esquerda.\n");
            // Lógica para virar à esquerda
            gpio_put(PIN_MOTOR_LEFT, 0);
            gpio_put(PIN_MOTOR_RIGHT, 1);
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        // Retornar o controle para a tarefa de movimentação
        obstacle_detected = false;
        vTaskResume(moveTaskHandle);

        // Resetar as variáveis
        left_distance = 9999;
        right_distance = 9999;
        scan_count = 0;
    }
}

int main() {
    stdio_init_all();
    setup_sensor();

    servoAttach(&myServo, SERVO_PIN);
    servoWrite(&myServo, 90);
    // Criação da fila de distâncias
    distanceQueue = xQueueCreate(QUEUE_SIZE, sizeof(float));
    if (distanceQueue == NULL) {
        printf("Erro ao criar a fila.\n");
        return -1;
    }

    // Criação das tarefas
    xTaskCreate(vTaskMove, "Move Task", 256, NULL, 2, &moveTaskHandle);
    xTaskCreate(vTaskScan, "Scan Task", 256, NULL, 1, &scanTaskHandle);
    xTaskCreate(vTaskDecision, "Decision Task", 256, NULL, 1, &decisionTaskHandle);
    xTaskCreate(vTaskMeasureDistance, "Measure Task", 256, NULL, 3, &measureTaskHandle);  // Nova tarefa de medição

    // Iniciar o agendador do FreeRTOS
    vTaskStartScheduler();

    while (1) {
        // O código não deve chegar aqui
    }

    return 0;
}