
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include <string.h>

// Definições dos pinos
#define LED_VERMELHO 13
#define LED_VERDE 11
#define BOTAO_PEDESTRE_A 5
#define BOTAO_PEDESTRE_B 6
#define BUZZER 21

// Pinos I2C do OLED
#define I2C_SDA 14
#define I2C_SCL 15

// Área para renderizar no display
struct render_area frame_area;

volatile bool pedestre_acionou = false;

struct repeating_timer timer_botao;
struct repeating_timer timer_semaforo;

typedef enum {
    SEMAFORO_VERMELHO,
    SEMAFORO_VERDE,
    SEMAFORO_AMARELO,
    TRAVESSIA_AMARELO,
    TRAVESSIA_VERMELHO,
    TRAVESSIA_BUZZER,
    TRAVESSIA_FINAL,
    POS_TRAVESSIA_VERDE,
    ESPERANDO_TRAVESSIA  
} EstadoSemaforo;

EstadoSemaforo estado = SEMAFORO_VERMELHO;
int contador = 0;

// Protótipos
void inicializar_hardware();
void atualizar_display(const char *texto, int seg);
void iniciar_ciclo_semaforo();
void iniciar_modo_travessia();
bool callback_timer_botao(struct repeating_timer *t);
bool callback_timer_semaforo(struct repeating_timer *t);

// Função para desenhar texto escalado no display OLED (simplificada)
void ssd1306_draw_string_scaled(uint8_t *buffer, int x, int y, const char *text, int scale) {
    while (*text) {
        ssd1306_draw_char(buffer, x, y, *text);
        x += 6 * scale;
        text++;
    }
}

int main() {
    stdio_init_all();
    inicializar_hardware();

    i2c_init(i2c1, 400000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init();

    frame_area.start_column = 0;
    frame_area.end_column = ssd1306_width - 1;
    frame_area.start_page = 0;
    frame_area.end_page = ssd1306_n_pages - 1;
    calculate_render_area_buffer_length(&frame_area);

    add_repeating_timer_ms(100, callback_timer_botao, NULL, &timer_botao);
    iniciar_ciclo_semaforo();

    printf("Semaforo iniciado...\n");

    while (true) {
        tight_loop_contents();
    }
}

void inicializar_hardware() {
    gpio_init(LED_VERMELHO);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);
    gpio_init(LED_VERDE);
    gpio_set_dir(LED_VERDE, GPIO_OUT);

    gpio_init(BOTAO_PEDESTRE_A);
    gpio_set_dir(BOTAO_PEDESTRE_A, GPIO_IN);
    gpio_pull_up(BOTAO_PEDESTRE_A);

    gpio_init(BOTAO_PEDESTRE_B);
    gpio_set_dir(BOTAO_PEDESTRE_B, GPIO_IN);
    gpio_pull_up(BOTAO_PEDESTRE_B);

    gpio_init(BUZZER);
    gpio_set_dir(BUZZER, GPIO_OUT);
    gpio_put(BUZZER, 0);

    gpio_put(LED_VERMELHO, 0);
    gpio_put(LED_VERDE, 0);
}

void atualizar_display(const char *texto, int seg) {
    char contador_str[10];
    if (seg == 0) {
        snprintf(contador_str, sizeof(contador_str), " ");
    } else {
        snprintf(contador_str, sizeof(contador_str), "%d", seg);
    }

    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);

    ssd1306_draw_string_scaled(ssd, 20, 20, texto, 2);
    ssd1306_draw_string_scaled(ssd, 50, 40, contador_str, 2);

    render_on_display(ssd, &frame_area);
}

void iniciar_ciclo_semaforo() {
    cancel_repeating_timer(&timer_semaforo);

    estado = SEMAFORO_VERMELHO;
    contador = 10;

    gpio_put(LED_VERMELHO, 1);
    gpio_put(LED_VERDE, 0);

    add_repeating_timer_ms(-1000, callback_timer_semaforo, NULL, &timer_semaforo);
}

void iniciar_modo_travessia() {
    cancel_repeating_timer(&timer_semaforo);

    estado = TRAVESSIA_AMARELO;
    contador = 3;

    gpio_put(LED_VERMELHO, 1);
    gpio_put(LED_VERDE, 1);

    atualizar_display("AMARELO", contador);

    add_repeating_timer_ms(-1000, callback_timer_semaforo, NULL, &timer_semaforo);
}

void botao_pedestre() {
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    ssd1306_draw_string_scaled(ssd, 15, 10, "BOTAO", 2);
    ssd1306_draw_string_scaled(ssd, 15, 30, "PEDESTRES", 2);
    ssd1306_draw_string_scaled(ssd, 15, 45, "ACIONADO", 2);
    render_on_display(ssd, &frame_area);
}

bool callback_timer_botao(struct repeating_timer *t) {
    static bool aguardando_soltar = false;
    static absolute_time_t ultimo_acionamento = {0};
    const int DEBOUNCE_MS = 250;

    bool pressionado = (!gpio_get(BOTAO_PEDESTRE_A) || !gpio_get(BOTAO_PEDESTRE_B));

    if (pressionado) {
        if (!aguardando_soltar) {
            if (absolute_time_diff_us(ultimo_acionamento, get_absolute_time()) / 1000 > DEBOUNCE_MS) {
                if (estado != ESPERANDO_TRAVESSIA && estado < TRAVESSIA_AMARELO) {
                    estado = ESPERANDO_TRAVESSIA;
                    contador = 2;
                    cancel_repeating_timer(&timer_semaforo);
                    add_repeating_timer_ms(-1000, callback_timer_semaforo, NULL, &timer_semaforo);
                }
                aguardando_soltar = true;
                ultimo_acionamento = get_absolute_time();
            }
        }
    } else {
        aguardando_soltar = false;
    }

    return true;
}

bool callback_timer_semaforo(struct repeating_timer *t) {
    if (estado == ESPERANDO_TRAVESSIA) {
        botao_pedestre();
        if (contador == 0) {
            iniciar_modo_travessia();
        } else {
            contador--;
        }
        return true;
    }

    switch (estado) {
        case SEMAFORO_VERMELHO:
            if (contador == 0) {
                estado = SEMAFORO_VERDE;
                contador = 10;
                gpio_put(LED_VERMELHO, 0);
                gpio_put(LED_VERDE, 1);
            } else {
                atualizar_display("VERMELHO", contador);
                contador--;
            }
            break;

        case SEMAFORO_VERDE:
            if (contador == 0) {
                estado = SEMAFORO_AMARELO;
                contador = 3;
                gpio_put(LED_VERMELHO, 1);
                gpio_put(LED_VERDE, 1);
            } else {
                atualizar_display("VERDE", contador);
                contador--;
            }
            break;

        case SEMAFORO_AMARELO:
            if (contador == 0) {
                estado = SEMAFORO_VERMELHO;
                contador = 10;
                gpio_put(LED_VERMELHO, 1);
                gpio_put(LED_VERDE, 0);
            } else {
                atualizar_display("AMARELO", contador);
                contador--;
            }
            break;

        case TRAVESSIA_AMARELO:
            if (contador == 0) {
                estado = TRAVESSIA_VERMELHO;
                contador = 5;
                gpio_put(LED_VERMELHO, 1);
                gpio_put(LED_VERDE, 0);
            } else {
                atualizar_display("AMARELO", contador);
                contador--;
            }
            break;

        case TRAVESSIA_VERMELHO:
            if (contador == 0) {
                estado = TRAVESSIA_BUZZER;
                contador = 5;
            } else {
                uint8_t ssd[ssd1306_buffer_length];
                memset(ssd, 0, ssd1306_buffer_length);
                ssd1306_draw_string_scaled(ssd, 15, 30, "VERMELHO", 2);
                render_on_display(ssd, &frame_area);
                contador--;
            }
            break;

        case TRAVESSIA_BUZZER: {
            char texto[20];
            snprintf(texto, sizeof(texto), "FALTAM %d s", contador);

            uint8_t ssd[ssd1306_buffer_length];
            memset(ssd, 0, ssd1306_buffer_length);
            ssd1306_draw_string_scaled(ssd, 20, 25, texto, 2);
            render_on_display(ssd, &frame_area);

            gpio_put(BUZZER, contador % 2);

            if (contador == 0) {
                gpio_put(BUZZER, 0);
                estado = TRAVESSIA_FINAL;
                contador = 2;
            } else {
                contador--;
            }
            break;
        }

        case TRAVESSIA_FINAL: {
            uint8_t ssd[ssd1306_buffer_length];
            memset(ssd, 0, ssd1306_buffer_length);
            ssd1306_draw_string_scaled(ssd, 15, 20, "TRAVESSIA", 2);
            ssd1306_draw_string_scaled(ssd, 15, 40, "ENCERRADA", 2);
            render_on_display(ssd, &frame_area);

            if (contador == 0) {
                estado = POS_TRAVESSIA_VERDE;
                contador = 10;
                gpio_put(LED_VERMELHO, 0);
                gpio_put(LED_VERDE, 1);
            } else {
                contador--;
            }
            break;
        }

        case POS_TRAVESSIA_VERDE:
            if (contador == 0) {
                iniciar_ciclo_semaforo();
            } else {
                atualizar_display("VERDE", contador);
                contador--;
            }
            break;
    }

    return true;
}
