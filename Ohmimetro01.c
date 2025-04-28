/*

Ohmimetro com a BitdogLab
Residente: Theógenes Gabriel Araújo de Andrade

*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "pico/bootrom.h"  // Adicionado para reset_usb_boot
#include "lib/ssd1306.h"
#include "lib/font.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define OLED_ADDR 0x3C
#define ADC_PIN 28
#define Botao_A 5
#define botaoB 6

int R_conhecido = 10000;   // Resistor de 10k ohm
float ADC_VREF = 3.31;     // Tensão de referência do ADC
int ADC_RESOLUTION = 4095; // Resolução do ADC (12 bits)

volatile bool sw = false;
uint32_t ultimo_press = 0;


// Série E24 (valores normalizados)
const float serie_E24[] = {
    1.0, 1.1, 1.2, 1.3, 1.5, 1.6, 1.8, 2.0, 2.2, 2.4, 2.7, 3.0,
    3.3, 3.6, 3.9, 4.3, 4.7, 5.1, 5.6, 6.2, 6.8, 7.5, 8.2, 9.1
};

// Nomes das cores para o código de resistores
const char* nomes_cores[] = {
    "Preto", "Marrom", "Vermelho", "Laranja", "Amarelo",
    "Verde", "Azul", "Violeta", "Cinza", "Branco"
};

//Estrutura para o código de cores
typedef struct {
    uint8_t digito1;
    uint8_t digito2;
    uint8_t multiplicador;
} CodigoCores;

// Função para modo BOOTSEL com botão B
void gpio_irq_handler(uint gpio, uint32_t events) {
    reset_usb_boot(0, 0);
}


// Encontra o valor E24 mais próximo
float aproximar_E24(float valor) {
    if (valor <= 0) return 0;
    
    //calcula o expoente por arredondamento
    int expoente = (int)floor(log10(valor));
    float normalizado = valor / pow(10, expoente);
    
    float mais_proximo = serie_E24[0];
    float menor_diff = fabs(normalizado - mais_proximo);
    
    //Busca o valor normalizado mais próximo
    for (int i = 1; i < 24; i++) {
        float diff = fabs(normalizado - serie_E24[i]);
        if (diff < menor_diff) {
            menor_diff = diff;
            mais_proximo = serie_E24[i];
        }
    }
    
    //retorna o mais próximo na base 10
    return mais_proximo * pow(10, expoente);
}

// Determina o código de cores
CodigoCores calcular_cores(float resistencia) {
    CodigoCores cores;
    if (resistencia < 10.0f) {
        // Valores abaixo de 10Ω têm representação especial
        cores.digito1 = 0;
        cores.digito2 = (uint8_t)round(resistencia);
        cores.multiplicador = 0;
    } else {
        int expoente = (int)floor(log10(resistencia));
        float normalizado = resistencia / pow(10, expoente);
        
        // Ajusta para 2 dígitos significativos
        int valor = (int)round(normalizado * 10);
        cores.digito1 = valor / 10;
        if(valor % 10 == 0){
          cores.digito2 = valor%10;
        }else{
          cores.digito2 = (valor-3) % 10;
        }
        cores.multiplicador = expoente - 1;
        printf("%d\n", valor);
    }
    
    return cores;
}



void exibir_resistencia(ssd1306_t *disp, float resistencia) {
  char buffer[32];

  // Limpa o display
  ssd1306_fill(disp, false);

  // Cabeçalho
  ssd1306_draw_string(disp, "CEPEDI   TIC37", 8, 6);
  ssd1306_draw_string(disp, "EMBARCATECH", 20, 16);
  ssd1306_line(disp, 3, 25, 123, 25, true);

  // Mostra o valor da resistência
  if (resistencia >= 1000.0f) {
      sprintf(buffer, "%.2f kΩ", resistencia / 1000.0f);
  } else {
      sprintf(buffer, "%.1f Ω", resistencia - 30);
  }
  //caso a resistência seja maior que a faixa (100k), o valor é considerado sem resistor
  if(resistencia > 100000){
    strcpy(buffer, "sem resistor");
    ssd1306_draw_string(disp, buffer, 10, 38);
  }else{
    ssd1306_draw_string(disp, "Resistencia:", 10, 28);
    ssd1306_draw_string(disp, buffer, 10, 38);
  }
  // Atualiza o display
  ssd1306_send_data(disp);
}

void exibir_codigo_cores(ssd1306_t *disp, CodigoCores cores) {
    // Limpa o display
    ssd1306_fill(disp, false);
  char buffer1[32];
  char buffer2[32];
  char buffer3[32];

  // Cabeçalho para a área de cores (pode ser adaptado conforme a posição)
  sprintf(buffer1, "%s", nomes_cores[cores.digito1]);
  sprintf(buffer2, "%s", nomes_cores[cores.digito2]);

  //Desenha a primeira e segunda faixa
  ssd1306_draw_string(disp, "Cores:", 10,5);
  ssd1306_draw_string(disp, buffer1, 10, 20);
  ssd1306_draw_string(disp, buffer2, 10, 30);

  // A partir carrega o buffer com o valor do multiplicador
  switch(cores.multiplicador) {
      case 0:
        strcpy(buffer3, "Preto");
        break;
      case 1:
        strcpy(buffer3, "Marrom");
        break;
      case 2:
        strcpy(buffer3, "Vermelho");
        break;
      case 3:
        strcpy(buffer3, "Laranja");
        break;
      case 4:
        strcpy(buffer3, "Amarelo");
        break;
      case 5:
        strcpy(buffer3, "Verde");
        break;
  }
  //Imprime a faixa do multiplicador de cores
  ssd1306_draw_string(disp, buffer3, 10, 40);

  //desenha os terminais do resistor
  ssd1306_vline(disp, 101, 0, 12, true);
  ssd1306_line(disp, 101, 46, 101, 56, true);

  //desenha a caixa do resistor
  ssd1306_rect(disp, 12, 94, 14, 34, true, false);

  //desenha as faixas do resistor
  ssd1306_line(disp, 94, 18, 107, 18, true);
  ssd1306_line(disp, 94, 19, 107, 19, true);
  ssd1306_line(disp, 94, 25, 107, 25, true);
  ssd1306_line(disp, 94, 26, 107, 26, true);
  ssd1306_line(disp, 94, 35, 107, 35, true);
  ssd1306_line(disp, 94, 36, 107, 36, true);

  // Atualiza o display com as cores
  ssd1306_send_data(disp);
}


int main() {
    // Configuração do botão BOOTSEL
    stdio_init_all();
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // Configuração do botão A (troca de telas)
    gpio_init(Botao_A);
    gpio_set_dir(Botao_A, GPIO_IN);
    gpio_pull_up(Botao_A);
    // Inicialização I2C
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicialização do display 
    ssd1306_t ssd;
    ssd1306_init(&ssd, 128, 64, false, OLED_ADDR, I2C_PORT); 
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
    
    // Inicialização ADC
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(2);

    while (true) {

        // Leitura do ADC com média 
        float soma = 0.0f;
        for (int i = 0; i < 500; i++) {
            soma += adc_read();
            sleep_ms(1);
        }
        float media = soma / 500.0f;

        // Cálculo da resistência
        
        float R_x = (R_conhecido * media) / (ADC_RESOLUTION - media);
        float R_E24 = aproximar_E24(R_x);
        CodigoCores cores = calcular_cores(R_E24);


        if (!gpio_get(Botao_A)) {
          uint32_t agora = to_ms_since_boot(get_absolute_time());
          
          //Debounce
          if (agora - ultimo_press > 190) {
              ultimo_press = agora;
              sw = !sw;
          }
      }
        // Exibição no display
        if(!sw){
          
          //Tela que exibi os valores das resistências
          exibir_resistencia(&ssd, R_E24);
        }else{
          //Tela que exibi o código de cores
          exibir_codigo_cores(&ssd, cores);
          
        }
        sleep_ms(5);
    }
}