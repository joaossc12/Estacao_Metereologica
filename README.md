# Estação de Monitoramento Ambiental com Raspberry Pi Pico W

![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)
![Platform: Pico W](https://img.shields.io/badge/Platform-Pico%20W-green.svg)
![Language: C/C++](https://img.shields.io/badge/Language-C/C++-orange.svg)

Um projeto de sistema embarcado que transforma uma Raspberry Pi Pico W em uma estação meteorológica e de monitoramento ambiental completa, com sensores de temperatura, umidade e pressão, display visual e uma interface web para controle e visualização remota de dados.

## Funcionalidades Principais

* **📈 Monitoramento em Tempo Real:** Leitura contínua de temperatura, umidade (AHT20), pressão e altitude calculada (BMP280).
* **🌐 Servidor Web Embarcado:** Acesse os dados e configure o dispositivo remotamente a partir de qualquer navegador na mesma rede Wi-Fi.
* **📊 Interface REST/JSON:** Endpoints simples para obter dados (`/estado`) e configurar parâmetros (`/config`), facilitando a integração com outras aplicações.
* **🚨 Alarmes Configuráveis:** Defina limites de temperatura e umidade para acionar alertas visuais (LED RGB) e sonoros (buzzer).
* **💡 Feedback Visual:** Uma matriz de LEDs WS2812B exibe graficamente o nível de umidade, mudando de cor em caso de alarme.
* **⚙️ Controle Físico:** Botões para ativar/desativar alarmes e entrar em modo BOOTSEL para atualização de firmware fácil via USB.

---

## Hardware Necessário

| Componente | Descrição |
| :--- | :--- |
| **Microcontrolador** | Raspberry Pi Pico W |
| **Sensor de Temp./Umidade** | AHT20 (I²C) |
| **Sensor de Temp./Pressão** | BMP280 (I²C) |
| **Display** | Matriz de LEDs 8x8 WS2812B (ou fita de LEDs) |
| **Alerta Sonoro** | Buzzer Ativo ou Passivo (para PWM) |
| **Controles** | 2x Botões (Push-buttons) |
| **Componentes Adicionais** | Protoboard, Jumpers, Resistores (se necessário) |

---

## Mapeamento de Pinos (Pinout)

Este é o mapeamento de pinos utilizado no projeto. As atribuições podem ser alteradas no código-fonte, se necessário.

| Pino (GP) | Função | Componente Conectado | Barramento |
| :--- | :--- | :--- | :--- |
| **GP0** | I2C0 SDA | Sensor AHT20 | `i2c0` |
| **GP1** | I2C0 SCL | Sensor AHT20 | `i2c0` |
| **GP2** | I2C1 SDA | Sensor BMP280 | `i2c1` |
| **GP3** | I2C1 SCL | Sensor BMP280 | `i2c1` |
| **GP7** | Saída de Dados para LEDs | Matriz WS2812B | `PIO` |
| **GP21** | Saída PWM para Alerta Sonoro | Buzzer | `PWM` |
| **GP5** | Entrada - Botão A (Alterna Alarme)| Botão A | `GPIO` |
| **GP6** | Entrada - Botão B (Modo BOOTSEL) | Botão B | `GPIO` |
| **GP 12 e 13** | LED RGB| LED da Placa Pico W | `GPIO` |

---

## Como Compilar e Executar

### 1. Configuração do Ambiente

Para compilar este projeto, você precisará do **SDK C/C++ para Raspberry Pi Pico** devidamente configurado em sua máquina.

1.  Siga o guia oficial [**"Getting started with Raspberry Pi Pico"**](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf) para instalar as ferramentas necessárias (GCC Arm, CMake, etc.).
2.  Clone este repositório para o seu computador:
    ```bash
    git clone [URL_DO_SEU_REPOSITORIO]
    cd [NOME_DA_PASTA_DO_PROJETO]
    ```
3.  **Importante:** Crie um arquivo de configuração para o Wi-Fi. Na raiz do projeto, crie um arquivo chamado `wifi_config.h` com as suas credenciais:
    ```c
    // arquivo: wifi_config.h
    #ifndef WIFI_CONFIG_H
    #define WIFI_CONFIG_H

    #define WIFI_SSID "NOME_DA_SUA_REDE_WIFI"
    #define WIFI_PASSWORD "SENHA_DA_SUA_REDE_WIFI"

    #endif
    ```

### 2. Compilação do Projeto

Com o ambiente configurado, execute os seguintes comandos no terminal, a partir da pasta raiz do projeto:

```bash
# Criar a pasta de build
mkdir build
cd build

# Executar o CMake para configurar o projeto
# Certifique-se de que a variável PICO_SDK_PATH está definida no seu sistema
cmake ..

# Compilar o código
make
