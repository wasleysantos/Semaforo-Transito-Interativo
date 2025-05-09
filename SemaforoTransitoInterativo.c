#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

// Pinos LED RGB (BitDogLab)
#define LED_VERMELHO 13
#define LED_VERDE 11

// Botão e Buzzer
#define BOTAO_PEDESTRE 5
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

    // Cria um timer repetitivo para verificar botão (sem pooling)
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

// Interrupção por timer para ler botão (com debounce)
bool callback_timer(struct repeating_timer *t) {
    static bool aguardando_soltar = false;
    static absolute_time_t ultimo_acionamento = {0};
    const int DEBOUNCE_MS = 150; // 150 milissegundos

    if (!gpio_get(BOTAO_PEDESTRE)) {  // Botão pressionado
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

    gpio_init(BOTAO_PEDESTRE); gpio_set_dir(BOTAO_PEDESTRE, GPIO_IN);
    gpio_pull_up(BOTAO_PEDESTRE); // botão com pull-up interno

    gpio_init(BUZZER); gpio_set_dir(BUZZER, GPIO_OUT);

    gpio_put(BUZZER, 0);
    gpio_put(LED_VERMELHO, 0);
    gpio_put(LED_VERDE, 0);
   
}

// Função padrão do semáforo (vermelho → verde → amarelo)
void semaforo_padrao() {
    // Vermelho por 10s
    gpio_put(LED_VERMELHO, 1);
    gpio_put(LED_VERDE, 0);
    printf("Sinal vermelho\n");
    sleep_ms(10000);

    // Verde por 10s
    gpio_put(LED_VERMELHO, 0);
    gpio_put(LED_VERDE, 1);
    printf("Sinal verde\n");
    sleep_ms(10000);

    // Amarelo por 3s (vermelho + verde = amarelo)
    gpio_put(LED_VERMELHO, 1);
    gpio_put(LED_VERDE, 1);
    printf("Sinal amarelo\n");
    sleep_ms(3000);
}

// Modo de travessia do pedestre
void modo_travessia() {

    // Amarelo por 3s antes de parar o tráfego
    gpio_put(LED_VERDE, 1);
    gpio_put(LED_VERMELHO, 1);
    printf("Sinal amarelo\n");
    sleep_ms(3000);

    // Vermelho
    gpio_put(LED_VERDE, 0);
    printf("Sinal vermelho\n");

    // Contagem regressiva no serial com alerta sonoro
    for (int i = 10; i > 0; i--) {
        printf("Faça a Travessia em %d...\n", i);
        gpio_put(BUZZER, 1);
        sleep_ms(300);
        gpio_put(BUZZER, 0);
        sleep_ms(800);
        printf("Atravessia finalizada %d...\n", i);
    }

    // Após travessia, ciclo volta ao normal
    gpio_put(LED_VERDE, 1);
    gpio_put(LED_VERMELHO, 0);
    sleep_ms(10000);
    gpio_put(LED_VERDE, 1);
    gpio_put(LED_VERMELHO, 1);
    sleep_ms(3000);
}
