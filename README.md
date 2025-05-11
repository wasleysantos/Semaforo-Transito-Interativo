# 🚦 Semáforo Interativo

## 🧠 Funcionamento

O projeto simula um **semáforo inteligente com acionamento por pedestre**, utilizando lógica embarcada, botões físicos, sinais visuais e sonoros:

- 🔴 **Vermelho:** 10 segundos
- 🟢 **Verde:** 10 segundos
- 🟡 **Amarelo:** 3 segundos
- 🚫 **Travessia de pedestre:** 10 segundos com contagem regressiva e buzzer

## 🙋 Interatividade

Pedestres podem solicitar a travessia pressionando os **botões A ou B**. O sistema responde com:

- 🔴 O semáforo muda para **vermelho** por 10 segundos
- ⏱️ Inicia uma **contagem regressiva** visível
- 🔊 Emite sinal sonoro com buzzer para auxiliar **pessoas com deficiência visual**
- 🖥️ Mostra informações no **display OLED**:
  - Cor atual do sinal
  - Tempo restante
  - Status do botão de pedestre

## 📺 Sinalização Sonora e Visual

- **Buzzer**: emite bipes durante a contagem para indicar o tempo de travessia
- **OLED LCD**: exibe o estado do semáforo em tempo real

## 🧪 Simulador

Você pode testar o projeto diretamente no [Wokwi](https://wokwi.com/projects/430490801003324417):

[![Simulador Wokwi](https://github.com/user-attachments/assets/a9cc81d5-37bb-486f-b249-1e3c7123b1af)](https://wokwi.com/projects/430490801003324417)

---

## 📦 Recursos Utilizados

- BitDogLab / Raspberry pi / Simulação no Wokwi
- Display OLED com driver SSD1306
- Buzzer para sinalização sonora
- Botões físicos
- Temporizadores com `sleep_ms` ou `timers`
- Linguagem C com SDK do Pico

---

📌 Ideal para projetos de acessibilidade, sistemas embarcados e automação de tráfego urbano.

---

🛠️ Fique a vontade para clonar, testar, contribuir ou adaptar!

