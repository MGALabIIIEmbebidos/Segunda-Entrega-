#include <esp_now.h>
#include <WiFi.h>
#include "esp_wifi.h"

// Pin del LDR
#define LDR_PIN 32

// Canal WiFi
const uint8_t canal = 6;

// MAC del receptor (ajústala según el receptor real)
uint8_t receiverMAC[] = {0x5c, 0x01, 0x3b, 0x72, 0xf2, 0xcc};

// Estructura de envío
typedef struct {
  int ldrValue;
} struct_message_send;

struct_message_send sensorSend;

// Tarea para lectura y envío del LDR
void LDRTask(void *parameter) {
  for (;;) {
    int ldr = analogRead(LDR_PIN);
    sensorSend.ldrValue = ldr;

    Serial.print("Valor LDR: ");
    Serial.println(ldr);

    esp_err_t res = esp_now_send(receiverMAC, (uint8_t *)&sensorSend, sizeof(sensorSend));
    if (res != ESP_OK) {
      Serial.println("❌ Error al enviar datos");
    }

    vTaskDelay(pdMS_TO_TICKS(10000));  // cada 10 segundos
  }
}

// Callback de envío ESP-NOW
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.printf("Envío ESP-NOW: %s\n", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FALLÓ");
}

void setup() {
  Serial.begin(115200);
  pinMode(LDR_PIN, INPUT);

  WiFi.mode(WIFI_STA);

  Serial.print("Transmisor MAC STA: ");
  Serial.println(WiFi.macAddress());

  esp_wifi_set_channel(canal, WIFI_SECOND_CHAN_NONE);
  Serial.printf("Transmisor fuerza canal: %d\n", canal);

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Error al iniciar ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = canal;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("❌ Fallo al agregar peer");
    return;
  }

  // Crear tarea para manejar el LDR
  xTaskCreatePinnedToCore(
    LDRTask,      // Función de la tarea
    "LDRTask",    // Nombre
    2048,         // Stack
    NULL,         // Parámetros
    1,            // Prioridad
    NULL,         // Handle
    1             // Núcleo (puedes usar 0 también)
  );
}

void loop() {
  vTaskDelay(portMAX_DELAY);  // Nada que hacer aquí
}
