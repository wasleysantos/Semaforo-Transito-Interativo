// Bibliotecas padrão e específicas do Raspberry Pi Pico
#include <stdio.h>
#include "pico/stdlib.h"          // Funções básicas do Pico
#include "hardware/gpio.h"        // Controle dos pinos GPIO
#include "hardware/timer.h"       // Temporizadores
#include "hardware/adc.h"         // Conversor Analógico-Digital (ADC)
#include "hardware/dma.h"         // Controle de DMA (não utilizado neste trecho)
#include "hardware/i2c.h"         // Comunicação I2C
#include "ssd1306.h"              // Controle do display OLED SSD1306
#include <string.h>               // Funções de manipulação de strings

// Definições dos pinos conectados aos LEDs
#define LED_VERMELHO 13
#define LED_VERDE 11

// Definições dos pinos conectados aos botões e buzzer
#define BOTAO_PEDESTRE_A 5
#define BOTAO_PEDESTRE_B 6
#define BUZZER 21

// Canal ADC para leitura de temperatura interna do chip
#define CANAL_ADC_TEMPERATURA 4

// Pinos para comunicação I2C com o display OLED
const uint I2C_SDA = 14, I2C_SCL = 15;

// Variável global para controlar quando o botão de pedestre for acionado
volatile bool pedestre_acionou = false;

// Estrutura para área de renderização do display
struct render_area frame_area;

// Protótipos das funções utilizadas no código
void inicializar_hardware();
void semaforo_padrao();
void modo_travessia();
bool callback_timer(struct repeating_timer *t);

// Função que escreve texto escalado no display OLED
void ssd1306_draw_string_scaled(uint8_t *buffer, int x, int y, const char *text, int scale) {
    while (*text) {
        for (int dx = 0; dx < scale; dx++)
            for (int dy = 0; dy < scale; dy++)
                ssd1306_draw_char(buffer, x + dx, y + dy, *text);
        x += 6 * scale; // Avança a posição do caractere no eixo X
        text++;         // Vai para o próximo caractere
    }
}

int main() {
    stdio_init_all();               // Inicializa a entrada/saída padrão (USB serial)
    inicializar_hardware();         // Configura GPIOs e periféricos
    adc_select_input(CANAL_ADC_TEMPERATURA);  // Seleciona canal de leitura de temperatura interna

    // Inicializa barramento I2C para o display OLED
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(); // Inicializa o display OLED

    // Define a área onde será desenhado o conteúdo no display
    frame_area.start_column = 0;
    frame_area.end_column = ssd1306_width - 1;
    frame_area.start_page = 0;
    frame_area.end_page = ssd1306_n_pages - 1;
    calculate_render_area_buffer_length(&frame_area);

    // Cria um temporizador repetitivo a cada 100 ms que chama a função callback_timer
    struct repeating_timer timer;
    add_repeating_timer_ms(100, callback_timer, NULL, &timer);

    printf("Semaforo iniciado...\n");

    // Loop principal
    while (true) {
        if (pedestre_acionou) {
            // Se algum botão de pedestre foi pressionado, ativa modo travessia
            printf("-----------------------------\n");
            printf("Botão de Pedestres acionado\n");
            printf("-----------------------------\n");
            modo_travessia();
            pedestre_acionou = false;  // Reseta o estado
        } else {
            // Caso contrário, mantém o funcionamento normal do semáforo
            semaforo_padrao();
        }
    }
}

// Função chamada a cada 100ms para verificar o botão de pedestre
bool callback_timer(struct repeating_timer *t) {
    static bool aguardando_soltar = false;               // Indica se está esperando o botão ser solto
    static absolute_time_t ultimo_acionamento = {0};     // Tempo do último acionamento válido
    const int DEBOUNCE_MS = 250;                         // Tempo de debounce (250ms)

    // Verifica se qualquer um dos dois botões está pressionado (nível lógico baixo)
    bool pressionado = (!gpio_get(BOTAO_PEDESTRE_A) || !gpio_get(BOTAO_PEDESTRE_B));

    if (pressionado) {
        if (!aguardando_soltar) {
            // Verifica se passou o tempo de debounce desde o último acionamento
            if (absolute_time_diff_us(ultimo_acionamento, get_absolute_time()) / 1000 > DEBOUNCE_MS) {
                pedestre_acionou = true;                 // Sinaliza que um pedestre pediu travessia
                aguardando_soltar = true;                // Aguarda soltar o botão
                ultimo_acionamento = get_absolute_time();// Atualiza o tempo do acionamento
            }
        }
    } else {
        // Botão foi solto, pode detectar novo acionamento
        aguardando_soltar = false;
    }

    return true; // Mantém o temporizador ativo
}

// Inicializa os periféricos do hardware
void inicializar_hardware() {
    // Configura os LEDs como saída
    gpio_init(LED_VERMELHO); 
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);
    gpio_init(LED_VERDE); 
    gpio_set_dir(LED_VERDE, GPIO_OUT);

    // Configura os botões como entrada com pull-up ativado
    gpio_init(BOTAO_PEDESTRE_A); 
    gpio_set_dir(BOTAO_PEDESTRE_A, GPIO_IN); 
    gpio_pull_up(BOTAO_PEDESTRE_A);

    gpio_init(BOTAO_PEDESTRE_B); 
    gpio_set_dir(BOTAO_PEDESTRE_B, GPIO_IN); 
    gpio_pull_up(BOTAO_PEDESTRE_B);

    // Configura o buzzer como saída
    gpio_init(BUZZER); 
    gpio_set_dir(BUZZER, GPIO_OUT);

    // Inicializa os estados: tudo desligado
    gpio_put(BUZZER, 0);
    gpio_put(LED_VERMELHO, 0);
    gpio_put(LED_VERDE, 0);
}

// Executa o ciclo padrão do semáforo com contagem no OLED
void semaforo_padrao() {
    // Acende LED vermelho
    gpio_put(LED_VERMELHO, 1);
    gpio_put(LED_VERDE, 0);
    for (int i = 10; i > 0; i--) {
        // Exibe "VERMELHO" e contador no display
        char texto[20];
        snprintf(texto, sizeof(texto), "%d", i);
        uint8_t ssd[ssd1306_buffer_length];
        memset(ssd, 0, ssd1306_buffer_length);
        ssd1306_draw_string_scaled(ssd, 20, 20, "VERMELHO", 2);
        ssd1306_draw_string_scaled(ssd, 50, 40, texto, 2);
        render_on_display(ssd, &frame_area);
        printf("Sinal vermelho(%d)\n", i);
        sleep_ms(1000);
        if (pedestre_acionou) return;  // Interrompe se pedestre apertar botão
    }

    // Acende LED verde
    gpio_put(LED_VERMELHO, 0);
    gpio_put(LED_VERDE, 1);
    for (int i = 10; i > 0; i--) {
        // Exibe "VERDE" e contador no display
        char texto[20];
        snprintf(texto, sizeof(texto), "%d", i);
        uint8_t ssd[ssd1306_buffer_length];
        memset(ssd, 0, ssd1306_buffer_length);
        ssd1306_draw_string_scaled(ssd, 40, 20, "VERDE", 2);
        ssd1306_draw_string_scaled(ssd, 50, 40, texto, 2);
        render_on_display(ssd, &frame_area);
        printf("Sinal verde(%d)\n", i);
        sleep_ms(1000);
        if (pedestre_acionou) return;
    }

    // Transição: amarelo (acende ambos LEDs)
    gpio_put(LED_VERMELHO, 1);
    gpio_put(LED_VERDE, 1);
    for (int i = 3; i > 0; i--) {
        // Exibe "AMARELO" e contador
        char texto[20];
        snprintf(texto, sizeof(texto), "%d", i);
        uint8_t ssd[ssd1306_buffer_length];
        memset(ssd, 0, ssd1306_buffer_length);
        ssd1306_draw_string_scaled(ssd, 20, 20, "AMARELO", 2);
        ssd1306_draw_string_scaled(ssd, 50, 40, texto, 2);
        render_on_display(ssd, &frame_area);
        printf("Sinal amarelo(%d)\n", i);
        sleep_ms(1000);
        if (pedestre_acionou) return;
    }
}

// Mostra a mensagem "Botão de pedestre acionado" no display
void mensagem_botao() {
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    ssd1306_draw_string_scaled(ssd, 40, 10, "BOTAO", 2);
    ssd1306_draw_string_scaled(ssd, 20, 25, "PEDESTRE", 2);
    ssd1306_draw_string_scaled(ssd, 20, 45, "ACIONADO", 2);
    render_on_display(ssd, &frame_area);
}

// Modo de travessia para pedestres
void modo_travessia() {     
    mensagem_botao();  // Informa que o botão foi pressionado

    // Fase de transição: amarelo
    gpio_put(LED_VERDE, 1);
    gpio_put(LED_VERMELHO, 1);
    printf("Sinal amarelo\n");
    sleep_ms(3000);

    // Para veículos: vermelho aceso, verde apagado
    gpio_put(LED_VERDE, 0);
    printf("Sinal vermelho\n");
    sleep_ms(5000);

    // Travessia com buzzer e contagem regressiva
    for (int i = 5; i > 0; i--) {
        printf("Travessia termina em %d...\n", i);
        char texto[20];
        snprintf(texto, sizeof(texto), "FALTAM %d s", i);
        uint8_t ssd[ssd1306_buffer_length];
        memset(ssd, 0, ssd1306_buffer_length);
        ssd1306_draw_string_scaled(ssd, 20, 25, texto, 2);
        render_on_display(ssd, &frame_area);
        gpio_put(BUZZER, 1);    // Ativa buzzer
        sleep_ms(300);
        gpio_put(BUZZER, 0);    // Desativa buzzer
        sleep_ms(800);
    }

    // Mensagem de travessia encerrada
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    ssd1306_draw_string_scaled(ssd, 15, 20, "TRAVESSIA", 2);
    ssd1306_draw_string_scaled(ssd, 15, 40, "ENCERRADA", 2);
    render_on_display(ssd, &frame_area);
    printf("Travessia encerrada\n");
    sleep_ms(2000); // Pausa antes de retornar ao fluxo normal

    // Reinício do ciclo: verde e vermelho acesos por 10s
    gpio_put(LED_VERDE, 1);
    gpio_put(LED_VERMELHO, 0);
    for (int i = 10; i > 0; i--) {
        char texto[20];
        snprintf(texto, sizeof(texto), "%d", i);
        uint8_t ssd[ssd1306_buffer_length];
        memset(ssd, 0, ssd1306_buffer_length);
        ssd1306_draw_string_scaled(ssd, 40, 20, "VERDE", 2);
        ssd1306_draw_string_scaled(ssd, 50, 40, texto, 2);
        render_on_display(ssd, &frame_area);
        printf("Sinal verde(%d)\n", 10 - i);
        sleep_ms(1000);
    }

    // Finaliza com fase amarela
    gpio_put(LED_VERDE, 1);
    gpio_put(LED_VERMELHO, 1);
    for (int i = 3; i > 0; i--) {
        char texto[20];
        snprintf(texto, sizeof(texto), "%d", i);
        uint8_t ssd[ssd1306_buffer_length];
        memset(ssd, 0, ssd1306_buffer_length);
        ssd1306_draw_string_scaled(ssd, 20, 20, "AMARELO", 2);
        ssd1306_draw_string_scaled(ssd, 50, 40, texto, 2);
        render_on_display(ssd, &frame_area);
        printf("Sinal amarelo(%d)\n", 3 - i);
        sleep_ms(1000);
    }
}
