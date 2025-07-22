//Bibliotecas Básicas
#include <stdio.h>
#include "pico/stdlib.h"
#include <math.h>


//Bibliotecas Interfaces
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"


//Bibliotecas Sensores
#include "aht20.h"
#include "bmp280.h"


// Bibliotecas para Wi-Fi
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lib/html_server.h"

#include "wifi_access.h"

// Macros sensores
#define I2C0_PORT i2c0              // i2c0 pinos 0 e 1
#define I2C0_SDA 0                  // 0
#define I2C0_SCL 1                  // 1
#define I2C1_PORT i2c1              // i2c1 pinos 2 e 3
#define I2C1_SDA 2                  // 2
#define I2C1_SCL 3                  // 3
#define SEA_LEVEL_PRESSURE 101325.0 // 101325.0 // Pressão ao nível do mar em Pa

//Macros Periféricos
#define LED_RED         13
#define LED_BLUE         12

//Biblioteca Matriz de LED
#include "lib/ws2812b.h"

// Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define BUTTON_B         6
#define BUTTON_A         5


static float limite_max = 100.0f; //Variáveis para controlar a matriz de LEDs
static float limite_min = 00.0f; //Variáveis para controlar a matriz de LEDs
// Cores da Matriz para cada estado do programa
const static Rgb std_color = {0, 0, 1};
const static Rgb alarme_color = {1, 0, 0};


volatile static bool alarme_on = false; // Estado do alarme

#define BUZZER  21
#define WRAP 1000
#define DIV_CLK 250
static uint slice;
volatile static bool buzzer_on = false;


typedef struct
{
    double temp_bmp;
    double altitude_bmp;
    double pressao_bmp;
    double temp_ath;
    double umidade_ath;

} dados_t;

dados_t dados;
struct http_state
{
    char response[20000];
    size_t len;
    size_t sent;
};

//Limites Iniciais
float limite_temp_max    = 40.0;   // Limites Iniciais
float limite_umidade_min = 30.0;   // Limites Iniciais
float calibracao_temp    =  0.0;   // Limites Iniciais

/*------------------------- Protótipo -------------------------*/
void matriz_show_lvl(double leitura); // Mostra nível de água na matriz de forma proporcional ao tanque
bool is_same_color(Rgb color1, Rgb color2); // Identifica se duas cores são iguais
double calculate_altitude(double pressure); // Função para calcular a altitude 
void buzzer_callback(); //Dispara buzzer
void gpio_irq_handler(uint gpio, uint32_t events); //Função de callback botões
void verifica_parametros(); //Função que verifica condições e adiciona OffSet


static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err);
static void start_http_server(void);




// Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"


int main()
{
    stdio_init_all();

    // Para ser utilizado o modo BOOTSEL com botão B
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    // Inicializa a Matriz
    matriz_init();

    // Inicializa o Buzzer
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(BUZZER);

    pwm_set_wrap(slice, WRAP);
    pwm_set_clkdiv(slice, DIV_CLK); 
    pwm_set_gpio_level(BUZZER, 0);
    pwm_set_enabled(slice, true);

    // Inicializa LED Vermelho
    gpio_init(LED_RED);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_put(LED_RED, 0); // Começa desligado

    // Inicializa LED Azul
    gpio_init(LED_BLUE);
    gpio_set_dir(LED_BLUE, GPIO_OUT);
    gpio_put(LED_BLUE, 0); // Começa desligado
    
    // Inicializa o I2C 0 para o AHT20
    i2c_init(I2C0_PORT, 400 * 1000);
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA);
    gpio_pull_up(I2C0_SCL);

    // Inicializa o I2C 1 para o SMP280
    i2c_init(I2C1_PORT, 400 * 1000);
    gpio_set_function(I2C1_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA);
    gpio_pull_up(I2C1_SCL);

    // Inicializa o BMP280
    bmp280_init(I2C1_PORT);
    struct bmp280_calib_param params;
    bmp280_get_calib_params(I2C1_PORT, &params);

    // Inicializa o AHT20
    aht20_reset(I2C0_PORT);
    aht20_init(I2C0_PORT);

    // Estruturas para leitura de sensores
    AHT20_Data data;
    int32_t raw_temp_bmp;
    int32_t raw_pressure;
    double altitude;

    printf("PROGRAMA INICIANDO!\n");


    if (cyw43_arch_init())
    {
        printf("Falha ao inicializar a arquitetura CYW43\n ");
        sleep_ms(2000);
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 50000))
    {
        printf("ERRO AO CONECTAR WIFI!\n");
        gpio_put(LED_RED,1);
        return 1;
    }

    gpio_put(LED_BLUE,1);
    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    printf("Wi-Fi conectado! IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    
    start_http_server();

    while (true) {
        // Polling para manter conexão
        cyw43_arch_poll();

        //Leitura do BMP280
        bmp280_read_raw(I2C1_PORT, &raw_temp_bmp, &raw_pressure);
        int32_t temperature = bmp280_convert_temp(raw_temp_bmp, &params);
        int32_t pressure = bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &params);
        // Cálculo da altitude
        double altitude = calculate_altitude(pressure);
        printf("Temperatura BMP: %d C\n", temperature);
        printf("Altitude BMP: %.2f\n", altitude);

        // Leitura do AHT20
        if (aht20_read(I2C0_PORT, &data))
        {
            printf("Temperatura AHT: %.2f C\n", data.temperature);
            printf("Umidade: %.2f %%\n", data.humidity);
        }
        else
        {
            printf("Erro na leitura do AHT10!\n\n\n");
        }
        dados.temp_bmp = (double) temperature/100;
        dados.pressao_bmp = (double)pressure;
        dados.altitude_bmp= (double)altitude;
        dados.temp_ath = (double) data.temperature;
        dados.umidade_ath = (double) data.humidity;

        verifica_parametros();

        printf("Limite Temp. Max %f \n Limite Umid. Min %f \n Offset Temp %f \n\n\n", limite_temp_max, limite_umidade_min, calibracao_temp);

        matriz_show_lvl(dados.umidade_ath);
        sleep_ms(1000);


    }
    cyw43_arch_deinit();
    return 0;
}

void verifica_parametros(){
    dados.temp_ath +=  calibracao_temp;
    if (dados.temp_ath > limite_temp_max || dados.umidade_ath < limite_umidade_min || alarme_on){
        gpio_put(LED_RED,1);
        buzzer_on = true;

        
    }else{
        buzzer_on = false;
        gpio_put(LED_RED,0);
    }
    buzzer_callback();

}

void gpio_irq_handler(uint gpio, uint32_t events)
{
    static absolute_time_t last_time_A = 0;
    static absolute_time_t last_time_B = 0;

    absolute_time_t now = get_absolute_time();
    if (gpio == BUTTON_A){ 
        if (absolute_time_diff_us(last_time_A, now) > 200000) { //Debouncing
            alarme_on = !alarme_on;
            last_time_A = now;
        }
    }else if (gpio == BUTTON_B){
        if (absolute_time_diff_us(last_time_B, now) > 200000) { //Debouncing
            reset_usb_boot(0, 0);
            last_time_B = now;
        }
}}
void buzzer_callback()
{   
 
   if (buzzer_on)
    {
        pwm_set_gpio_level(BUZZER, WRAP / 2);
    }
    else
    {
        pwm_set_gpio_level(BUZZER, 0);
    }



}

void matriz_show_lvl(double leitura)
{
    static uint8_t prev_lines = 0;
    static Rgb prev_matriz_color = {0, 0, 1};
    static uint8_t min_lines = 0; // Define o valor mínimo de linhas a serem desenhadas na matriz

    // Limitar leitura ao intervalo
    leitura = fmaxf(limite_min, fminf(leitura, limite_max));
   
    // Mapeia o valor lido para o total de linhas adicionais na matriz
    uint8_t lines_bruto = ((leitura - limite_min) / (limite_max - limite_min)) * (MATRIZ_ROWS - min_lines);
    uint8_t lines = round(lines_bruto) + min_lines; // Arredonda valor obtido

    // Define a cor baseado no estado do programa
    Rgb matriz_color = alarme_on ? alarme_color : std_color;

    // Atualiza a matriz APENAS se houver informações novas a mostrar
    if (lines != prev_lines || !is_same_color(matriz_color, prev_matriz_color))
    {   
        // Desenha linhas proporcionais ao nível da água atual na matriz, simulando um tanque
        matriz_clear();
        for (int i = 0; i < lines; i++)
        {
            matriz_draw_linha(0, MATRIZ_ROWS - 1 - i, 5, matriz_color);
        }
        matriz_send_data();

        prev_lines = lines;
        prev_matriz_color = matriz_color;
    }
}
bool is_same_color(Rgb color1, Rgb color2)
{
    return color1.r == color2.r && color1.g == color2.g && color1.b == color2.b;
}
double calculate_altitude(double pressure)
{
    return (44330.0 * (1.0 - pow(pressure / SEA_LEVEL_PRESSURE, 0.1903)));
}

// Função de callback para aceitar conexões TCP
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}

// Função de callback para enviar dados HTTP
static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;
    if (hs->sent >= hs->len)
    {
        tcp_close(tpcb);
        free(hs);
    }
    return ERR_OK;
}

// Função para iniciar o servidor HTTP
static void start_http_server(void)
{
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb)
    {
        printf("Erro ao criar PCB TCP\n");
        return;
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP rodando na porta 80...\n");
}

static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    // Informa à pilha TCP que consumimos p->tot_len bytes
    tcp_recved(tpcb, p->tot_len);

    char *req = (char *)p->payload;
    struct http_state *hs = malloc(sizeof(*hs));
    if (!hs) {
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_MEM;
    }
    hs->sent = 0;

    // 1) GET /estado → devolve JSON com medições + configurações
    if (strncmp(req, "GET /estado", 11) == 0) {
        char json_body[256];
        int json_len = snprintf(json_body, sizeof(json_body),
            "{\"pressao_bmp\":%.2f,\"temp_ath\":%.2f,\"umidade_ath\":%.2f,"
            "\"maxTemp\":%.1f,\"minHum\":%.1f,\"calTemp\":%.1f}",
            dados.pressao_bmp, dados.temp_ath, dados.umidade_ath,
            limite_temp_max, limite_umidade_min, calibracao_temp
        );

        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Cache-Control: no-store\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            json_len, json_body
        );
    }
    // 2) GET /config?maxTemp=&minHum=&calTemp=
    else if (strncmp(req, "GET /config?", 12) == 0) {
        char *pMax = strstr(req, "maxTemp=");
        char *pMin = strstr(req, "minHum=");
        char *pCal = strstr(req, "calTemp=");
        if (pMax && pMin && pCal) {
            limite_temp_max    = atof(pMax + 8);
            limite_umidade_min = atof(pMin + 7);
            calibracao_temp    = atof(pCal + 8);
        }
        char cfg_body[128];
        int cfg_len = snprintf(cfg_body, sizeof(cfg_body),
            "{\"maxTemp\":%.1f,\"minHum\":%.1f,\"calTemp\":%.1f}",
            limite_temp_max, limite_umidade_min, calibracao_temp
        );

        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Cache-Control: no-store\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            cfg_len, cfg_body
        );
    }
    // 3) Qualquer outra rota → HTML
    else {
        size_t html_len = strlen(html_data);
        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %zu\r\n"
            "Cache-Control: no-store\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            html_len, html_data
        );
    }

    // Enfileira envio e configura callbacks
    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);
    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    // Libera o pbuf
    pbuf_free(p);
    return ERR_OK;
}