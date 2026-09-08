/*
  ESTACAO METEOROLOGICA - NO REMOTO (sensores + LoRa TX)
  Heltec WiFi LoRa 32 V3/V4 - chip radio SX1262

  Le BMP280 (I2C dedicado - SDA=41, SCL=42), DHT22 (GPIO 7) e
  GUVA-S12SD (saida analogica - GPIO 4) a cada INTERVALO_LEITURA
  e transmite os dados via LoRa para a "base receptora"
  (outra Heltec ligada na Orange Pi).

  Bibliotecas necessarias (Gerenciador de Bibliotecas):
    - Adafruit BMP280 Library
    - Adafruit Unified Sensor
    - DHT sensor library (Adafruit)
    - RadioLib (Jan Gromes)
    - U8g2 (oliver)
*/

#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>
#include <RadioLib.h>
#include <U8g2lib.h>

// ---------- CONFIGURACAO ----------
#define SDA_SENSOR   41
#define SCL_SENSOR   42
#define DHT_PIN      7
#define DHT_TYPE     DHT22
#define UV_PIN       4      // GUVA-S12SD (saida analogica) - pino ADC livre
const unsigned long INTERVALO_LEITURA = 60000UL;  // 60s entre leituras

// LoRa (SX1262 - pinos padrao Heltec WiFi LoRa 32 V3/V4)
#define LORA_NSS     8
#define LORA_DIO1    14
#define LORA_RST     12
#define LORA_BUSY    13
#define LORA_FREQ    915.0   // MHz - faixa permitida no Brasil (ANATEL)
#define LORA_SF      9       // Spreading Factor: equilibrio alcance/velocidade
#define LORA_POWER   20      // dBm - potencia de saida

// Alimentacao do OLED (Vext - ativo em LOW no Heltec V3)
#define VEXT_PIN     36

// ---------- OBJETOS ----------
TwoWire I2C_SENSOR = TwoWire(1);
Adafruit_BMP280 bmp(&I2C_SENSOR);
DHT dht(DHT_PIN, DHT_TYPE);
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /*reset=*/21, /*clock=*/18, /*data=*/17);

int enviosOk = 0;
int enviosFalha = 0;

// Converte a leitura do GUVA-S12SD (tensao em mV) em Indice UV (0-11+),
// usando a tabela de referencia mais comum para esse sensor.
// Valido para uso educacional/aproximado, nao para medicao de precisao.
float tensaoParaIndiceUV(float mv) {
  const float tabela_mv[]  = {0, 227, 318, 408, 503, 606, 696, 795, 881, 976, 1079, 1170};
  const float tabela_idx[] = {0, 1,   2,   3,   4,   5,   6,   7,   8,   9,   10,   11};
  const int n = 12;

  if (mv <= tabela_mv[0]) return 0;
  if (mv >= tabela_mv[n - 1]) return tabela_idx[n - 1];

  for (int i = 0; i < n - 1; i++) {
    if (mv >= tabela_mv[i] && mv <= tabela_mv[i + 1]) {
      float frac = (mv - tabela_mv[i]) / (tabela_mv[i + 1] - tabela_mv[i]);
      return tabela_idx[i] + frac * (tabela_idx[i + 1] - tabela_idx[i]);
    }
  }
  return 0;
}

float lerIndiceUV() {
  int leituraBruta = analogRead(UV_PIN);           // 0-4095 (ADC 12 bits)
  float tensao_mv = (leituraBruta / 4095.0) * 3300.0; // mV, referencia 3.3V
  return tensaoParaIndiceUV(tensao_mv);
}

void oledMsg(String l1, String l2 = "", String l3 = "") {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 12, l1.c_str());
  if (l2 != "") u8g2.drawStr(0, 28, l2.c_str());
  if (l3 != "") u8g2.drawStr(0, 44, l3.c_str());
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  delay(300);

  // Liga alimentacao do OLED
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
  delay(100);

  u8g2.begin();
  oledMsg("Estacao Meteo", "No Remoto", "Iniciando...");

  I2C_SENSOR.begin(SDA_SENSOR, SCL_SENSOR);
  if (!bmp.begin(0x76)) {
    Serial.println("ERRO: BMP280 nao encontrado no endereco 0x76!");
    oledMsg("ERRO", "BMP280 nao", "encontrado");
    delay(3000);
  }
  dht.begin();

  analogReadResolution(12);
  pinMode(UV_PIN, INPUT);

  int estado = radio.begin(LORA_FREQ);
  if (estado != RADIOLIB_ERR_NONE) {
    Serial.print("ERRO ao iniciar LoRa, codigo: ");
    Serial.println(estado);
    oledMsg("ERRO LoRa", "codigo:", String(estado));
    while (true) delay(1000);
  }
  radio.setOutputPower(LORA_POWER);
  radio.setSpreadingFactor(LORA_SF);

  oledMsg("Estacao Meteo", "No Remoto", "Pronto!");
  delay(1000);
}

void loop() {
  float temperatura = bmp.readTemperature();
  float pressao = bmp.readPressure() / 100.0F;  // Pa -> hPa
  float umidade = dht.readHumidity();
  float indiceUV = lerIndiceUV();

  if (isnan(umidade)) {
    Serial.println("Aviso: falha ao ler DHT22");
    umidade = -1;
  }

  String json = "{";
  json += "\"temperatura\":" + String(temperatura, 1) + ",";
  json += "\"pressao\":" + String(pressao, 1) + ",";
  json += "\"umidade\":" + String(umidade, 1) + ",";
  json += "\"uv_index\":" + String(indiceUV, 1);
  json += "}";

  Serial.println(json);

  int estado = radio.transmit(json);
  if (estado == RADIOLIB_ERR_NONE) {
    enviosOk++;
    Serial.println("[OK] Enviado via LoRa");
  } else {
    enviosFalha++;
    Serial.print("[FALHA] Codigo: ");
    Serial.println(estado);
  }

  oledMsg(
    "T:" + String(temperatura, 1) + "C  U:" + String(umidade, 1) + "%",
    "P:" + String(pressao, 1) + "hPa  UV:" + String(indiceUV, 1),
    "OK:" + String(enviosOk) + " Falha:" + String(enviosFalha)
  );

  delay(INTERVALO_LEITURA);
}
