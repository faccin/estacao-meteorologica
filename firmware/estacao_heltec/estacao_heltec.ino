/*
 * ═══════════════════════════════════════════════════════════════
 *  Estação Meteorológica Educacional — Firmware Heltec V4
 * ═══════════════════════════════════════════════════════════════
 *
 * Hardware:
 *   - Heltec WiFi LoRa 32 V3 (ESP32-S3)
 *   - BMP280 (pressão + temperatura) via I2C
 *   - DHT22 (umidade + temperatura) via pino digital
 *
 * Funcionamento:
 *   1. Conecta no WiFi da Orange Pi (hotspot "EstacaoMeteo")
 *   2. Lê os sensores a cada 60 segundos
 *   3. Mostra no display OLED
 *   4. Envia via HTTP POST para o servidor Flask
 *
 * Bibliotecas necessárias (instalar via Arduino IDE):
 *   - Adafruit BMP280 Library
 *   - Adafruit Unified Sensor
 *   - DHT sensor library (Adafruit)
 *   - ArduinoJson
 *   - Heltec ESP32 Dev-Boards (board manager)
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include "HT_SSD1306Wire.h"   // Display OLED do Heltec V3

// ── Configuração WiFi ────────────────────────────────────────
const char* WIFI_SSID     = "EstacaoMeteo";
const char* WIFI_PASSWORD = "meteorologia";

// ── Configuração do servidor ─────────────────────────────────
const char* SERVIDOR_URL = "http://192.168.4.1:5000/api/dados";

// ── Intervalo entre leituras (ms) ────────────────────────────
const unsigned long INTERVALO_LEITURA = 60000;  // 60 segundos

// ── Pinos ────────────────────────────────────────────────────
// I2C do Heltec V3: SDA = 41, SCL = 42
#define SDA_PIN   41
#define SCL_PIN   42
#define DHT_PIN   7    // GPIO7 para o DHT22
#define DHT_TIPO  DHT22

// ── Display OLED do Heltec V3 ────────────────────────────────
// Construtor: endereço, SDA, SCL, geometria, frequência, RST
SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// ── Sensores ─────────────────────────────────────────────────
Adafruit_BMP280 bmp;
DHT dht(DHT_PIN, DHT_TIPO);

// ── Variáveis globais ────────────────────────────────────────
float temperatura = 0;
float umidade     = 0;
float pressao     = 0;
bool  bmp_ok      = false;
unsigned long ultimaLeitura  = 0;
unsigned long ultimoEnvio    = 0;
int   enviosOk    = 0;
int   enviosFalha = 0;

// ══════════════════════════════════════════════════════════════
//  SETUP
// ══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Estação Meteorológica Educacional ===\n");

  // ── Inicializar display OLED ───────────────────────────────
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.flipScreenVertically();
  mostrarMensagem("Iniciando...");

  // ── Inicializar I2C e BMP280 ───────────────────────────────
  Wire.begin(SDA_PIN, SCL_PIN);

  if (bmp.begin(0x76)) {       // endereço padrão
    bmp_ok = true;
    Serial.println("[OK] BMP280 encontrado (0x76)");
  } else if (bmp.begin(0x77)) { // endereço alternativo
    bmp_ok = true;
    Serial.println("[OK] BMP280 encontrado (0x77)");
  } else {
    Serial.println("[ERRO] BMP280 não encontrado!");
    mostrarMensagem("ERRO: BMP280\nnao encontrado!\nVerifique as\nconexoes I2C");
    delay(3000);
  }

  if (bmp_ok) {
    // Configuração para leitura meteorológica
    bmp.setSampling(
      Adafruit_BMP280::MODE_NORMAL,
      Adafruit_BMP280::SAMPLING_X2,   // temperatura
      Adafruit_BMP280::SAMPLING_X16,  // pressão (alta resolução)
      Adafruit_BMP280::FILTER_X16,    // filtro
      Adafruit_BMP280::STANDBY_MS_500
    );
  }

  // ── Inicializar DHT22 ─────────────────────────────────────
  dht.begin();
  Serial.println("[OK] DHT22 inicializado");

  // ── Conectar WiFi ─────────────────────────────────────────
  conectarWiFi();

  // ── Primeira leitura ──────────────────────────────────────
  lerSensores();
  enviarDados();
}

// ══════════════════════════════════════════════════════════════
//  LOOP
// ══════════════════════════════════════════════════════════════
void loop() {
  unsigned long agora = millis();

  // Reconectar WiFi se caiu
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  // Ler e enviar no intervalo configurado
  if (agora - ultimaLeitura >= INTERVALO_LEITURA) {
    lerSensores();
    enviarDados();
    ultimaLeitura = agora;
  }

  // Atualizar display a cada 5 segundos
  static unsigned long ultimoDisplay = 0;
  if (agora - ultimoDisplay >= 5000) {
    atualizarDisplay();
    ultimoDisplay = agora;
  }

  delay(100);
}

// ══════════════════════════════════════════════════════════════
//  FUNÇÕES
// ══════════════════════════════════════════════════════════════

void conectarWiFi() {
  Serial.printf("Conectando em '%s'...\n", WIFI_SSID);
  mostrarMensagem("Conectando WiFi...\n" + String(WIFI_SSID));

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 30) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[OK] WiFi conectado! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[ERRO] Falha ao conectar WiFi");
    mostrarMensagem("WiFi FALHOU!\nVerifique se a\nOrange Pi esta\nligada");
    delay(3000);
  }
}

void lerSensores() {
  // ── BMP280: pressão e temperatura ──────────────────────────
  if (bmp_ok) {
    pressao     = bmp.readPressure() / 100.0;  // Pa → hPa
    temperatura = bmp.readTemperature();
  }

  // ── DHT22: umidade (e temperatura como backup) ─────────────
  float u = dht.readHumidity();
  float t = dht.readTemperature();

  if (!isnan(u)) {
    umidade = u;
  }

  // Se BMP280 falhou, usar temperatura do DHT22
  if (!bmp_ok && !isnan(t)) {
    temperatura = t;
  }

  // ── Log serial ─────────────────────────────────────────────
  Serial.printf("Leitura: T=%.1f°C  U=%.1f%%  P=%.1f hPa\n",
                temperatura, umidade, pressao);

  // ── Também envia pela serial como JSON (debug) ─────────────
  StaticJsonDocument<128> doc;
  doc["temperatura"] = round(temperatura * 10) / 10.0;
  doc["umidade"]     = round(umidade * 10) / 10.0;
  doc["pressao"]     = round(pressao * 10) / 10.0;
  serializeJson(doc, Serial);
  Serial.println();
}

void enviarDados() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[AVISO] Sem WiFi — dados não enviados");
    enviosFalha++;
    return;
  }

  // ── Montar JSON ────────────────────────────────────────────
  StaticJsonDocument<128> doc;
  doc["temperatura"] = round(temperatura * 10) / 10.0;
  doc["umidade"]     = round(umidade * 10) / 10.0;
  doc["pressao"]     = round(pressao * 10) / 10.0;

  String json;
  serializeJson(doc, json);

  // ── Enviar via HTTP POST ───────────────────────────────────
  HTTPClient http;
  http.begin(SERVIDOR_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  int httpCode = http.POST(json);

  if (httpCode == 201) {
    Serial.println("[OK] Dados enviados com sucesso");
    enviosOk++;
  } else {
    Serial.printf("[ERRO] HTTP %d\n", httpCode);
    enviosFalha++;
  }

  http.end();
}

void atualizarDisplay() {
  display.clear();

  // ── Cabeçalho ──────────────────────────────────────────────
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "Est. Meteorologica");

  // Indicador WiFi
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  if (WiFi.status() == WL_CONNECTED) {
    display.drawString(128, 0, "WiFi OK");
  } else {
    display.drawString(128, 0, "SEM WiFi");
  }

  display.drawHorizontalLine(0, 12, 128);

  // ── Valores ────────────────────────────────────────────────
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);

  // Temperatura
  display.drawString(0, 16, String(temperatura, 1) + " C");

  // Umidade
  display.drawString(72, 16, String(umidade, 0) + " %");

  // Pressão
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 36, "Pressao: " + String(pressao, 1) + " hPa");

  // ── Rodapé: status ─────────────────────────────────────────
  display.drawHorizontalLine(0, 50, 128);
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  // Contador de envios
  String status = "OK:" + String(enviosOk) + " Falha:" + String(enviosFalha);
  display.drawString(0, 53, status);

  // Tempo desde último envio
  if (ultimaLeitura > 0) {
    int segs = (millis() - ultimaLeitura) / 1000;
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(128, 53, String(segs) + "s");
  }

  display.display();
}

void mostrarMensagem(String msg) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  // Quebra a mensagem em linhas
  int y = 10;
  int inicio = 0;
  for (int i = 0; i <= msg.length(); i++) {
    if (i == msg.length() || msg[i] == '\n') {
      display.drawString(0, y, msg.substring(inicio, i));
      y += 14;
      inicio = i + 1;
    }
  }

  display.display();
}
