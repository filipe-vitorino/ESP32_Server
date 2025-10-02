#include "wifi_handler.h"
#include "config_handler.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "data_logger.h"
#include "time.h"

// --- Configurações da Rede Wi-Fi ---
const char* ssid = "ESP32_Sensor_Server";
const char* password = "12345678";

// --- Configurações de Hora (NTP) ---
// Sem horário de verão

// Objeto do servidor web
AsyncWebServer server(80);

void setupWiFi() {
  Serial.println("Configurando modo Access Point (AP)...");
  // Inicia o AP com o SSID e senha definidos
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Sincroniza a hora do ESP32 com um servidor de tempo na internet
  // Isso é essencial para que os timestamps dos logs estejam corretos

  // Endpoint para dados em tempo real
  server.on("/dados", HTTP_GET, [](AsyncWebServerRequest *request){
  Serial.println("Request de dados");

    StaticJsonDocument<200> doc;
    doc["vazao"] = random(100, 200) / 10.0;
    doc["temperatura"] = random(200, 300) / 10.0;
    
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });

  // Endpoint para o histórico completo, com filtro opcional por timestamp
  server.on("/historico", HTTP_GET, [](AsyncWebServerRequest *request){
    int page = 1;
    int limit = 50; // Vamos buscar 50 registos de cada vez

    // Verifica se o app enviou os parâmetros de página e limite
    if (request->hasParam("page")) {
        page = request->getParam("page")->value().toInt();
    }
    if (request->hasParam("limit")) {
        limit = request->getParam("limit")->value().toInt();
    }

    Serial.printf("Requisição para /historico recebida. Página: %d, Limite: %d\n", page, limit);

    // Chama a nova função que lê apenas a página necessária
    String pageContent = getLogPage(page, limit);
    
    // Envia a página como resposta
    request->send(200, "application/json", pageContent);
  });


  // Endpoint para apagar o arquivo de log (estratégia "Apagar Após Sincronizar")
  server.on("/limpar_historico", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Reutiliza a constante LOG_FILE do data_logger
    extern const char* LOG_FILE; 
    if(SPIFFS.remove(LOG_FILE)) {
      request->send(200, "text/plain", "Historico apagado com sucesso.");
      Serial.println("Arquivo de log apagado via comando Wi-Fi.");
    } else {
      request->send(500, "text/plain", "Falha ao apagar historico.");
      Serial.println("Falha ao apagar arquivo de log via comando Wi-Fi.");
    }
  });

  server.on("/test-stream", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginChunkedResponse("application/json",
      [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        static int i = 0;

        if (i == 0) {
          // abre colchete JSON
          const char *start = "[";
          memcpy(buffer, start, 1);
          i++;
          return 1;
        }

        if (i <= 1000) {
          StaticJsonDocument<128> doc;
          doc["id"] = i;
          doc["value"] = random(0, 100);
          doc["vazao"] = random(100, 200) / 10.0;
          doc["temperatura"] = random(200, 300) / 10.0;

          String out;
          serializeJson(doc, out);

          if (i > 1) out = "," + out;  // vírgula entre objetos

          size_t len = out.length();
          if (len > maxLen) len = maxLen;

          memcpy(buffer, out.c_str(), len);
          i++;
          return len;
        } else if (i == 1001) {
          // fecha colchete JSON
          const char *end = "]";
          memcpy(buffer, end, 1);
          i++;
          return 1;
        } else {
          // terminou
          i = 0; 
          return 0;
        }
      }
    );

    request->send(response);
  });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Requisição para /config recebida.");
    String config = getConfigJsonString();
    request->send(200, "application/json", config);
  });

  server.begin();
  Serial.println("Servidor Web iniciado.");
}