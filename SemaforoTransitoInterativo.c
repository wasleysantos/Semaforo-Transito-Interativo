#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include <string.h>

// Pinos dos LEDs (BitDogLab)
#define LED_VERMELHO 13
#define LED_VERDE 11

// Botões e Buzzer
#define BOTAO_PEDESTRE_A 5
#define BOTAO_PEDESTRE_B 6
#define BUZZER 21

#define CANAL_ADC_TEMPERATURA 4

const uint I2C_SDA = 14, I2C_SCL = 15;

// Estado do sistema
volatile bool pedestre_acionou = false;

// Buffer OLED
struct render_area frame_area;

// Funções
void inicializar_hardware();
void semaforo_padrao();
void modo_travessia();
bool callback_timer(struct repeating_timer *t);

// Desenha texto no display com escala
void ssd1306_draw_string_scaled(uint8_t *buffer, int x, int y, const char *text, int scale) {
    while (*text) {
        for (int dx = 0; dx < scale; dx++)
            for (int dy = 0; dy < scale; dy++)
                ssd1306_draw_char(buffer, x + dx, y + dy, *text);
        x += 6 * scale;
        text++;
    }
}

// Limpa a tela do display OLED
void limpar_tela() {
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);
}

int main() {
    stdio_init_all();
    inicializar_hardware();
    adc_select_input(CANAL_ADC_TEMPERATURA);

    // Inicializa I2C e OLED
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
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

    struct repeating_timer timer;
    add_repeating_timer_ms(100, callback_timer, NULL, &timer);

    printf("Semaforo iniciado...\n");

    while (true) {
        uint8_t ssd[ssd1306_buffer_length];
        memset(ssd, 0, ssd1306_buffer_length);
        ssd1306_draw_string_scaled(ssd, 20, 20, "SEMAFORO", 2);
        ssd1306_draw_string_scaled(ssd, 10, 40, "INTERATIVO", 2);
        render_on_display(ssd, &frame_area);

        if (pedestre_acionou) {
            printf("-----------------------------\n");
            printf("Botão de Pedestres acionado\n");
            printf("-----------------------------\n");

            uint8_t ssd[ssd1306_buffer_length];
            memset(ssd, 0, ssd1306_buffer_length);
            ssd1306_draw_string_scaled(ssd, 40, 10, "BOTAO", 2);
            ssd1306_draw_string_scaled(ssd, 20, 25, "PEDESTRE", 2);
            ssd1306_draw_string_scaled(ssd, 20, 45, "ACIONADO", 2);
            render_on_display(ssd, &frame_area);

            modo_travessia();
            pedestre_acionou = false;
        } else {
            semaforo_padrao();
        }
    }
}

bool callback_timer(struct repeating_timer *t) {
    static bool aguardando_soltar = false;
    static absolute_time_t ultimo_acionamento = {0};
    const int DEBOUNCE_MS = 250;

    bool pressionado = (!gpio_get(BOTAO_PEDESTRE_A) || !gpio_get(BOTAO_PEDESTRE_B));

    if (pressionado) {
        if (!aguardando_soltar) {
            if (absolute_time_diff_us(ultimo_acionamento, get_absolute_time()) / 1000 > DEBOUNCE_MS) {
                pedestre_acionou = true;
                aguardando_soltar = true;
                ultimo_acionamento = get_absolute_time();
            }
        }
    } else {
        aguardando_soltar = false;
    }

    return true;
}

void inicializar_hardware() {
    gpio_init(LED_VERMELHO); gpio_set_dir(LED_VERMELHO, GPIO_OUT);
    gpio_init(LED_VERDE); gpio_set_dir(LED_VERDE, GPIO_OUT);

    gpio_init(BOTAO_PEDESTRE_A); gpio_set_dir(BOTAO_PEDESTRE_A, GPIO_IN); gpio_pull_up(BOTAO_PEDESTRE_A);
    gpio_init(BOTAO_PEDESTRE_B); gpio_set_dir(BOTAO_PEDESTRE_B, GPIO_IN); gpio_pull_up(BOTAO_PEDESTRE_B);

    gpio_init(BUZZER); gpio_set_dir(BUZZER, GPIO_OUT);

    gpio_put(BUZZER, 0);
    gpio_put(LED_VERMELHO, 0);
    gpio_put(LED_VERDE, 0);
}

void semaforo_padrao() {
    gpio_put(LED_VERMELHO, 1);
    gpio_put(LED_VERDE, 0);
    for (int i = 0; i < 10; i++) {
        printf("Sinal vermelho(%d)\n", 10 - i);
        sleep_ms(1000);
        if (pedestre_acionou) return;
    }

    gpio_put(LED_VERMELHO, 0);
    gpio_put(LED_VERDE, 1);
    for (int i = 0; i < 10; i++) {
        printf("Sinal verde(%d)\n", 10 - i);
        sleep_ms(1000);
        if (pedestre_acionou) return;
    }

    gpio_put(LED_VERMELHO, 1);
    gpio_put(LED_VERDE, 1);
    for (int i = 0; i < 3; i++) {
        printf("Sinal amarelo(%d)\n", 3 - i);
        sleep_ms(1000);
        if (pedestre_acionou) return;
    }
}

void modo_travessia() {
    gpio_put(LED_VERDE, 1);
    gpio_put(LED_VERMELHO, 1);
    printf("Sinal amarelo\n");
    sleep_ms(3000);

    gpio_put(LED_VERDE, 0);
    printf("Sinal vermelho\n");
    sleep_ms(5000);

    for (int i = 5; i > 0; i--) {
        printf("Travessia termina em %d...\n", i);

        char texto[20];
        snprintf(texto, sizeof(texto), "FALTAM %d s", i);
        uint8_t ssd[ssd1306_buffer_length];
        memset(ssd, 0, ssd1306_buffer_length);
        ssd1306_draw_string_scaled(ssd, 20, 25, texto, 2);
        render_on_display(ssd, &frame_area);
        gpio_put(BUZZER, 1);
        sleep_ms(300);
        gpio_put(BUZZER, 0);
        sleep_ms(800);
    }
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    ssd1306_draw_string_scaled(ssd, 15, 20, "TRAVESSIA", 2);
    ssd1306_draw_string_scaled(ssd, 15, 40, "ENCERRADA", 2);
    render_on_display(ssd, &frame_area);
    printf("Travessia encerrada\n");

    gpio_put(LED_VERDE, 1);
    gpio_put(LED_VERMELHO, 0);
    for (int i = 0; i < 10; i++) {
        printf("Sinal verde(%d)\n", 10 - i);
        sleep_ms(1000);
    }

    gpio_put(LED_VERDE, 1);
    gpio_put(LED_VERMELHO, 1);
    for (int i = 0; i < 3; i++) {
        printf("Sinal amarelo(%d)\n", 3 - i);
        sleep_ms(1000);
    }
}
