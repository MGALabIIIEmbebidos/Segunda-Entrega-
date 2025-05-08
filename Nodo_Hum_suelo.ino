#include <esp_now.h>
#include <WiFi.h>
#include "esp_wifi.h"

// ——— Configuración WiFi ———
const char* ssid     = "iPhone";
const char* password = "25310tati";

// ——— Sensor de Humedad ———
const int SOIL_PIN     = 34;
const int VALOR_SECO   = 2590;
const int VALOR_MOJADO = 1200;

// ——— MAC del receptor ———
uint8_t receiverMAC[] = {0x5C, 0x01, 0x3B, 0x72, 0xF2, 0xCC};

// ——— Estructura de envío ———
typedef struct {
  float soil;
} struct_message_send;

struct_message_send sensorSend;

// ——— Callback de envío ESP-NOW ———
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.printf("Envío ESP-NOW: %s\n", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FALLÓ");
}

// ——— Tarea que lee el sensor y envía datos ———
void SoilSensorTask(void *parameter) {
  for (;;) {
    int raw = analogRead(SOIL_PIN);
    int pct = map(raw, VALOR_SECO, VALOR_MOJADO, 0, 100);
    pct = constrain(pct, 0, 100);

    Serial.printf("RAW=%4d  →  %3d%%\n", raw, pct);

    sensorSend.soil = pct;
    esp_err_t res = esp_now_send(receiverMAC, (uint8_t *)&sensorSend, sizeof(sensorSend));
    if (res != ESP_OK) {
      Serial.printf("Error al enviar datos (err %d)\n", res);
    }

    vTaskDelay(pdMS_TO_TICKS(10000));  // cada 10 segundos
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);
  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Conectado. IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("Transmisor MAC STA: ");
  Serial.println(WiFi.macAddress());

  uint8_t canal = WiFi.channel();
  Serial.printf("Transmisor fuerza canal: %d\n", canal);
  esp_wifi_set_channel(canal, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error al iniciar ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = canal;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Fallo al agregar peer");
    return;
  }

  // tarea 
  xTaskCreatePinnedToCore(
    SoilSensorTask,
    "SoilSensorTask",
    2048,
    NULL,
    1,
    NULL,
    1  
  );
}

void loop() {
  vTaskDelay(portMAX_DELAY);  
}
