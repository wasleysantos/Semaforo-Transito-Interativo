#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

// Pinos dos LEDs (BitDogLab)
#define LED_VERMELHO 13
#define LED_VERDE 11

// Botões e Buzzer
#define BOTAO_PEDESTRE_A 5
#define BOTAO_PEDESTRE_B 6
#define BUZZER 21

// Estado do sistema
volatile bool pedestre_acionou = false;

// Funções
void inicializar_hardware();
void semaforo_padrao();
void modo_travessia();
bool callback_timer(struct repeating_timer *t);

int main() {
    stdio_init_all();
    inicializar_hardware();

    // Cria um timer repetitivo para verificar os botões com debounce
    struct repeating_timer timer;
    add_repeating_timer_ms(100, callback_timer, NULL, &timer);

    printf("Sistema do Sinal iniciado...\n");

    while (true) {
        if (pedestre_acionou) {
            printf("Botão de Pedestres acionado\n");
            printf("Aguarde o sinal ficar vermelho e faça a travessia\n");
            modo_travessia();
            pedestre_acionou = false;
        } else {
            semaforo_padrao();
        }
    }
}

// Interrupção por timer para ler botões (com debounce para A ou B)
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

// Inicialização dos GPIOs
void inicializar_hardware() {
    gpio_init(LED_VERMELHO); gpio_set_dir(LED_VERMELHO, GPIO_OUT);
    gpio_init(LED_VERDE); gpio_set_dir(LED_VERDE, GPIO_OUT);

    gpio_init(BOTAO_PEDESTRE_A); gpio_set_dir(BOTAO_PEDESTRE_A, GPIO_IN);
    gpio_pull_up(BOTAO_PEDESTRE_A);
    gpio_init(BOTAO_PEDESTRE_B); gpio_set_dir(BOTAO_PEDESTRE_B, GPIO_IN);
    gpio_pull_up(BOTAO_PEDESTRE_B);

    gpio_init(BUZZER); gpio_set_dir(BUZZER, GPIO_OUT);

    gpio_put(BUZZER, 0);
    gpio_put(LED_VERMELHO, 0);
    gpio_put(LED_VERDE, 0);
}

// Semáforo com verificação do botão a cada segundo
void semaforo_padrao() {
    // Vermelho por 10s
    gpio_put(LED_VERMELHO, 1);
    gpio_put(LED_VERDE, 0);
    for (int i = 0; i < 10; i++) {
        printf("Sinal vermelho\n");
        sleep_ms(1000);
        if (pedestre_acionou) return;
    }

    // Verde por 10s
    gpio_put(LED_VERMELHO, 0);
    gpio_put(LED_VERDE, 1);
    for (int i = 0; i < 10; i++) {
        printf("Sinal verde\n");
        sleep_ms(1000);
        if (pedestre_acionou) return;
    }

    // Amarelo por 3s (vermelho + verde)
    gpio_put(LED_VERMELHO, 1);
    gpio_put(LED_VERDE, 1);
    for (int i = 0; i < 3; i++) {
        printf("Sinal amarelo\n");
        sleep_ms(1000);
        if (pedestre_acionou) return;
    }
}

// Modo de travessia do pedestre
void modo_travessia() {
    // Amarelo por 3s antes do vermelho
    gpio_put(LED_VERDE, 1);
    gpio_put(LED_VERMELHO, 1);
    printf("Sinal amarelo\n");
    sleep_ms(3000);

    // Vermelho para veículos
    gpio_put(LED_VERDE, 0);
    printf("Sinal vermelho\n");
    sleep_ms(5000);

    // Contagem regressiva com som no buzzer
    for (int i = 5; i > 0; i--) {
        printf("Atenção!! Travessia termina em %d...\n", i);
        gpio_put(BUZZER, 1);
        sleep_ms(300);
        gpio_put(BUZZER, 0);
        sleep_ms(800);
    }

    // Volta ao verde e amarelo
    gpio_put(LED_VERDE, 1);
    gpio_put(LED_VERMELHO, 0);
    for (int i = 0; i < 10; i++) {
        printf("Sinal verde\n");
        sleep_ms(1000);
        if (pedestre_acionou) return;
    }

    gpio_put(LED_VERDE, 1);
    gpio_put(LED_VERMELHO, 1);
    for (int i = 0; i < 3; i++) {
        printf("Sinal amarelo\n");
        sleep_ms(1000);
        if (pedestre_acionou) return;
    }
}
