#include "room_control.h"

#include "gpio.h"    // Para controlar LEDs
#include "systick.h" // Para obtener ticks y manejar tiempos
#include "uart.h"    // Para enviar mensajes
#include "tim.h"     // Para controlar el PWM
#include <stdio.h>
#include <stdbool.h>

// Estados de la sala
typedef enum {
    ROOM_IDLE,
    ROOM_OCCUPIED
} room_state_t;

// Variable de estado global
room_state_t current_state = ROOM_IDLE;
static uint32_t led_on_time = 0;

static uint8_t current_duty = PWM_INITIAL_DUTY;
static uint8_t prev_duty_before_b1 = 0;
static uint32_t restore_deadline_ms = 0;
static bool restore_active = false;

static bool gradual_active = false;
static uint8_t gradual_level = 0;
static uint32_t gradual_next_ms = 0;

static void set_duty_percent(uint8_t pct)
{
    if (pct > 100) pct = 100;
    current_duty = pct;
    tim3_ch1_pwm_set_duty_cycle((uint32_t)pct);
}

     

void room_control_app_init(void) {

    tim3_ch1_pwm_set_duty_cycle(PWM_INITIAL_DUTY);
    uart_send_string("Controlador de Sala v2.0\r\n");
    uart_send_string("Estado inicial:\r\n");
    uart_send_string(" - Lampara: 20%\r\n");
    uart_send_string(" - Puerta: Cerrada\r\n"); 
}

void room_control_on_button_press(void)
{
    prev_duty_before_b1 = current_duty;
    set_duty_percent(100);
    restore_deadline_ms = systick_get_ms() + 10000U;
    restore_active = true;
    uart_send_string("B1: Lampara al 100% por 10s, luego se restaura.\r\n");
}

void room_control_on_uart_receive(char received_char)
{
    switch (received_char) {
        case 'h':
        case 'H':
            tim3_ch1_pwm_set_duty_cycle(100);
            uart_send_string("PWM: 100%\r\n");
            break;
        case 'l':
        case 'L':
            tim3_ch1_pwm_set_duty_cycle(0);
            uart_send_string("PWM: 0%\r\n");
            break;
        case 'O':
        case 'o':
            current_state = ROOM_OCCUPIED;
            tim3_ch1_pwm_set_duty_cycle(100);
            led_on_time = systick_get_ms();
            uart_send_string("Sala ocupada\r\n");
            break;
        case 'I':
        case 'i':
            current_state = ROOM_IDLE;
            tim3_ch1_pwm_set_duty_cycle(0);
            uart_send_string("Sala vacía\r\n");
            break;
        case '1':
            tim3_ch1_pwm_set_duty_cycle(10);
            uart_send_string("PWM: 10%\r\n");
            break;
        case '2':
            tim3_ch1_pwm_set_duty_cycle(20);
            uart_send_string("PWM: 20%\r\n");
            break;
        case '3':
            tim3_ch1_pwm_set_duty_cycle(30);
            uart_send_string("PWM: 30%\r\n");
            break;
        case '4':
            tim3_ch1_pwm_set_duty_cycle(40);
            uart_send_string("PWM: 40%\r\n");
            break;
        case '5':
            tim3_ch1_pwm_set_duty_cycle(50);
            uart_send_string("PWM: 50%\r\n");
            break;

        case 's':
        case 'S': {
            char buf[80];
            const char *door = (current_state == ROOM_OCCUPIED) ? "Abierta" : "Cerrada";
            snprintf(buf, sizeof(buf), "Estado actual:\r\n - Lampara: %u%%\r\n - Puerta: %s\r\n", (unsigned)current_duty, door);
            uart_send_string(buf);
            break;
        }

        case '?':
            uart_send_string("Comandos disponibles:\r\n");
            uart_send_string(" '1'-'5': Ajustar brillo lampara (10%, 20%, 30%, 40%, 50%)\r\n");
            uart_send_string(" '0'   : Apagar lampara\r\n");
            uart_send_string(" 'o'   : Abrir puerta (ocupar sala)\r\n");
            uart_send_string(" 'c'   : Cerrar puerta (vaciar sala)\r\n");
            uart_send_string(" 's'   : Estado del sistema\r\n");
            uart_send_string(" '?'   : Ayuda\r\n");
            uart_send_string(" 'g'   : Transicion gradual de brillo\r\n");
            break;

        case 'g':
        case 'G':
            gradual_active = true;
            gradual_level = 0;
            set_duty_percent(0);
            gradual_next_ms = systick_get_ms() + 500U;
            uart_send_string("Transicion gradual iniciada.\r\n");
            break;

        default:
            uart_send_string("Comando desconocido: ");
            uart_send(received_char);
            uart_send_string("\r\n");
            break;
    }
}

void room_control_update(void)
{
    uint32_t now = systick_get_ms();

    if (restore_active) {
        if ((int32_t)(now - restore_deadline_ms) >= 0) {
            set_duty_percent(prev_duty_before_b1);
            restore_active = false;
            uart_send_string("Restaurado brillo previo.\r\n");
        }
    }

    if (gradual_active) {
        if ((int32_t)(now - gradual_next_ms) >= 0) {
            if (gradual_level < 10) {
                gradual_level++;
                set_duty_percent((uint8_t)(gradual_level * 10));
                gradual_next_ms = now + 500U;
            } else {
                gradual_active = false;
                uart_send_string("Transicion gradual completada.\r\n");
            }
        }
    }
    
    if (current_state == ROOM_OCCUPIED) {
        if (systick_get_ms() - led_on_time >= LED_TIMEOUT_MS) {
            current_state = ROOM_IDLE;
            tim3_ch1_pwm_set_duty_cycle(0);
            uart_send_string("Timeout: Sala vacía\r\n");
        }
    }
}