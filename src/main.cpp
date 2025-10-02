#include <Arduino.h>
#include "data_logger.h"
#include "ble_handler.h"
#include "wifi_handler.h"
#include  "config_handler.h"

const long LOG_INTERVAL = 30000; // Salva um novo dado a cada 30 segundos
unsigned long previousLogMillis = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("\nIniciando Sensor Hub...");

  setupDataLogger();
  setupConfig();
  setupBLE();
  setupWiFi();

  Serial.println("Setup concluído. O dispositivo está rodando.");
}

void loop() {
  loopBLE();

  unsigned long currentMillis = millis();
  if (currentMillis - previousLogMillis >= LOG_INTERVAL) {
    previousLogMillis = currentMillis;
    logFakeSensorData();
  }
}