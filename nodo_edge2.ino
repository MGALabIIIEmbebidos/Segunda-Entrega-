#include <WiFi.h>
#include <esp_now.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <time.h>
#include "esp_wifi.h"

// WiFi
const char* ssid     = "iPhone";
const char* password = "25310tati";

// Telegram
#define BOTtoken  "7672713502:AAH55Vyi1rU-EF1eij3dnpqYvRMLxzsE58g"
#define CHAT_ID   "5365006160"
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// Umbrales
#define TEMP_THRESHOLD      27.0
#define HUM_THRESHOLD       80.0
#define SOIL_THRESHOLD_LOW  30.0
#define SOIL_THRESHOLD_HIGH 80.0
#define LDR_THRESHOLD       700

// MACs
uint8_t dhtSenderMAC[]  = {0x08, 0xD1, 0xF9, 0xEE, 0x0A, 0x04};
uint8_t soilSenderMAC[] = {0xa0, 0xb7, 0x65, 0x1b, 0x18, 0x50};
uint8_t ldrSenderMAC[]  = {0xa0, 0xa3, 0xb3, 0x2a, 0xE0, 0x20};

// Estructuras
typedef struct { float temp, hum; } dht_message_t;
typedef struct { float soil; }      soil_message_t;
typedef struct { int ldrValue; }    light_message_t;

// Colas
QueueHandle_t dhtQueue;
QueueHandle_t soilQueue;
QueueHandle_t ldrQueue;
QueueHandle_t msgQueue;

// ——— Recepción ESP-NOW ———
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incoming, int len) {
  if (memcmp(info->src_addr, dhtSenderMAC, 6) == 0 && len == sizeof(dht_message_t)) {
    dht_message_t d;
    memcpy(&d, incoming, len);
    xQueueSend(dhtQueue, &d, portMAX_DELAY);
  } else if (memcmp(info->src_addr, soilSenderMAC, 6) == 0 && len == sizeof(soil_message_t)) {
    soil_message_t s;
    memcpy(&s, incoming, len);
    xQueueSend(soilQueue, &s, portMAX_DELAY);
  } else if (memcmp(info->src_addr, ldrSenderMAC, 6) == 0 && len == sizeof(light_message_t)) {
    light_message_t l;
    memcpy(&l, incoming, len);
    xQueueSend(ldrQueue, &l, portMAX_DELAY);
  }
}

// ——— Tareas por sensor ———
void DHTTask(void *parameter) {
  dht_message_t d;
  for (;;) {
    if (xQueueReceive(dhtQueue, &d, portMAX_DELAY)) {
      String msg = "";
      Serial.printf("DHT: T=%.2f°C, H=%.2f%%\n", d.temp, d.hum);
      if (d.temp > TEMP_THRESHOLD) msg += "⚠️ Temp ambiente alta: " + String(d.temp, 2) + "°C\n";
      if (d.hum  > HUM_THRESHOLD)  msg += "💧 Hum ambiente alta:  " + String(d.hum, 2) + "%\n";
      if (msg.length()) {
        char* buf = (char*)malloc(msg.length() + 1);
        msg.toCharArray(buf, msg.length() + 1);
        xQueueSend(msgQueue, &buf, portMAX_DELAY);
      }
    }
  }
}

void SoilTask(void *parameter) {
  soil_message_t s;
  for (;;) {
    if (xQueueReceive(soilQueue, &s, portMAX_DELAY)) {
      String msg = "";
      Serial.printf("Suelo: %.1f%%\n", s.soil);
      if      (s.soil < SOIL_THRESHOLD_LOW)  msg += "🌱 Suelo muy seco: " + String(s.soil, 1) + "%\n";
      else if (s.soil > SOIL_THRESHOLD_HIGH) msg += "💧 Suelo muy húmedo: " + String(s.soil, 1) + "%\n";
      if (msg.length()) {
        char* buf = (char*)malloc(msg.length() + 1);
        msg.toCharArray(buf, msg.length() + 1);
        xQueueSend(msgQueue, &buf, portMAX_DELAY);
      }
    }
  }
}

void LDRTask(void *parameter) {
  light_message_t l;
  for (;;) {
    if (xQueueReceive(ldrQueue, &l, portMAX_DELAY)) {
      String msg = "";
      Serial.printf("LDR: %d\n", l.ldrValue);
      if (l.ldrValue > LDR_THRESHOLD) msg += "🌑 Luz baja: " + String(l.ldrValue) + "\n";
      else                            msg += "💡 Luz normal: " + String(l.ldrValue) + "\n";
      char* buf = (char*)malloc(msg.length() + 1);
      msg.toCharArray(buf, msg.length() + 1);
      xQueueSend(msgQueue, &buf, portMAX_DELAY);
    }
  }
}

void TelegramTask(void *parameter) {
  char* recvBuffer;
  for (;;) {
    if (xQueueReceive(msgQueue, &recvBuffer, portMAX_DELAY)) {
      bool ok = bot.sendMessage(CHAT_ID, String(recvBuffer), "");
      Serial.println(ok ? "✅ Telegram enviado" : "❌ Error en Telegram");
      free(recvBuffer);
    }
  }
}

// ——— Setup principal ———
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nConectado IP: " + WiFi.localIP().toString());

  int canal = WiFi.channel();
  Serial.printf("Canal: %d\n", canal);
  configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  struct tm ti;
  while (!getLocalTime(&ti)) delay(200);
  Serial.printf("Hora: %02d:%02d:%02d\n", ti.tm_hour, ti.tm_min, ti.tm_sec);

  client.setInsecure();
  bot.sendMessage(CHAT_ID, "🤖 Bot receptor iniciado", "");

  // Crear colas
  dhtQueue  = xQueueCreate(5, sizeof(dht_message_t));
  soilQueue = xQueueCreate(5, sizeof(soil_message_t));
  ldrQueue  = xQueueCreate(5, sizeof(light_message_t));
  msgQueue  = xQueueCreate(5, sizeof(char*));

  // Crear tareas
  xTaskCreate(DHTTask,  "DHTTask",  4096, NULL, 1, NULL);
  xTaskCreate(SoilTask, "SoilTask", 4096, NULL, 1, NULL);
  xTaskCreate(LDRTask,  "LDRTask",  4096, NULL, 1, NULL);
  xTaskCreate(TelegramTask, "TelegramTask", 8192, NULL, 1, NULL);

  // ESP-NOW
  esp_wifi_set_channel(canal, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Error al iniciar ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);

  esp_now_peer_info_t peer = {};
  peer.channel = canal;
  peer.encrypt = false;

  memcpy(peer.peer_addr, dhtSenderMAC, 6);
  esp_now_add_peer(&peer);

  memcpy(peer.peer_addr, soilSenderMAC, 6);
  esp_now_add_peer(&peer);

  memcpy(peer.peer_addr, ldrSenderMAC, 6);
  esp_now_add_peer(&peer);
}

void loop() {
  vTaskDelay(portMAX_DELAY);  
}
