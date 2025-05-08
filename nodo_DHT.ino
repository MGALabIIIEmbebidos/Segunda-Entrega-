#include <esp_now.h>
#include <WiFi.h>
#include <DHT.h>
#include "esp_wifi.h"  // Para esp_wifi_set_channel()

// —— Configuración del DHT22 ——
#define DHTPIN 5
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// —— MAC del receptor ——
uint8_t receiverMAC[] = {0x5c, 0x01, 0x3b, 0x72, 0xf2, 0xcc};

// —— Estructura de datos a enviar ——
typedef struct {
  float temp;
  float hum;
} struct_message_send;

struct_message_send sensorSend;

// —— Callback de envío ——
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.printf("Envío ESP-NOW: %s\n", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FALLÓ");
}

// —— Tarea que lee el DHT22 y envía los datos ——
void DHTSensorTask(void *parameter) {
  for (;;) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (isnan(t) || isnan(h)) {
      Serial.println("Error al leer DHT22");
    } else {
      sensorSend.temp = t;
      sensorSend.hum  = h;

      Serial.printf("Temp: %.1f °C | Hum: %.1f %%\n", t, h);

      esp_err_t res = esp_now_send(receiverMAC, (uint8_t *)&sensorSend, sizeof(sensorSend));
      if (res != ESP_OK) {
        Serial.println("Error al enviar datos");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10000));  // cada 10 segundos
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  WiFi.mode(WIFI_STA);

  Serial.print("Transmisor MAC STA: ");
  Serial.println(WiFi.macAddress());

  const uint8_t canal = 6;
  esp_wifi_set_channel(canal, WIFI_SECOND_CHAN_NONE);
  Serial.printf("Transmisor fuerza canal: %d\n", canal);

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

  // Crear tarea de sensor DHT
  xTaskCreatePinnedToCore(
    DHTSensorTask,
    "DHTSensorTask",
    2048,
    NULL,
    1,
    NULL,
    1  
  );
}

void loop() {
  vTaskDelay(portMAX_DELAY);  // Nada que hacer aquí
}
