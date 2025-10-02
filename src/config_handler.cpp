#include "config_handler.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

const char* CONFIG_FILE = "/config.json";

void setupConfig() {
  if (SPIFFS.exists(CONFIG_FILE)) {
    Serial.println("Ficheiro de configuração encontrado.");
  } else {
    Serial.println("AVISO: Ficheiro de configuração não encontrado. Usando valores padrão.");
    // Aqui você poderia criar um ficheiro de configuração padrão se quisesse
  }
}

String getConfigJsonString() {
  File file = SPIFFS.open(CONFIG_FILE, "r");
  if (!file) {
    // Retorna um JSON de erro se o ficheiro não puder ser lido
    return "{\"error\":\"config_not_found\"}";
  }
  String config = file.readString();
  file.close();
  return config;
}