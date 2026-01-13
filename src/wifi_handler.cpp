#include "wifi_handler.h"

#include <WiFi.h>
#include <WebServer.h>
//#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "data_logger.h"
#include "time.h"
#include "hub_config.h"
#include "device_controller.h"
// --- Configurações da Rede Wi-Fi ---
const char* ssid = "ESP32_Sensor_Server";
const char* password = "12345678";


AsyncWebServer server(80);

#define MAX_LINE_LENGTH 256   // máximo de caracteres por linha
#define CHUNK_SAFE_SIZE 512   
// tamanho seguro do chunk
#define LINES_PER_PAGE 50
#define BUFFER_SIZE 256

struct StreamState {
  File root;
  File dateDir;
  File logFile;
  bool finished;
  int totalLines;
  int pageCount;
};

StreamState streamState = {File(), File(), File(), false, 0, 0};

struct FileIterator {
  File root;
  File dateDir;
  File logFile;
};


String escapeJSON(const String& input) {
    String output;
    for (size_t i = 0; i < input.length(); i++) {
        char c = input.charAt(i);
        if (c == '"' || c == '\\') {
            output += '\\';
        }
        output += c;
    }
    return output;
}


void enviarArquivoInteiro(AsyncWebServerRequest *request, const String& arquivo, int page, int totalArquivos) {
    File file = LittleFS.open(arquivo, "r");
    if (!file) {
        request->send(404, "application/json", "{\"erro\":\"Arquivo não encontrado\"}");
        return;
    }

    Serial.printf("Enviando arquivo completo: %s, Tamanho: %d bytes\n", arquivo.c_str(), file.size());

    // Usa variáveis estáticas para manter o estado entre chunks
    static bool headerSent = false;
    static bool inLinesArray = false;
    static int lineCount = 0;
    static File currentFile;

    // Reseta as variáveis estáticas para nova requisição
    headerSent = false;
    inLinesArray = false;
    lineCount = 0;
    currentFile = file;

    AsyncWebServerResponse *response = request->beginChunkedResponse("application/json", 
        [page, totalArquivos, arquivo](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            
            // Primeiro chunk: envia o cabeçalho
            if (!headerSent) {
                String header = "{\"pagina_atual\":" + String(page) + 
                               ",\"total_arquivos\":" + String(totalArquivos) +
                               ",\"arquivo\":\"" + arquivo + 
                               "\",\"tamanho\":" + String(currentFile.size()) + 
                               ",\"linhas\":[";
                size_t headerLen = header.length();
                if (headerLen <= maxLen) {
                    memcpy(buffer, header.c_str(), headerLen);
                    headerSent = true;
                    inLinesArray = true;
                    return headerLen;
                }
            }
            
            // Chunks intermediários: envia as linhas
            if (inLinesArray && currentFile.available()) {
                String linha = currentFile.readStringUntil('\n');
                linha.trim();
                
                if (linha != "") {
                    // CORREÇÃO: Usar método correto para construir a string
                    String jsonLine;
                    if (lineCount > 0) {
                        jsonLine = ",";
                    }
                      if (linha.startsWith("{")) {
                        jsonLine += linha;  // Já é JSON, usa diretamente
                    } else {
                        jsonLine += "\"" + escapeJSON(linha) + "\"";  // Não é JSON, escapa
                    }
                    
                    lineCount++;
                    
                    if (jsonLine.length() <= maxLen) {
                        memcpy(buffer, jsonLine.c_str(), jsonLine.length());
                        return jsonLine.length();
                    }
                }
                return 0;
            }
            
            // Último chunk: fecha o JSON
            if (inLinesArray && !currentFile.available()) {
                String footer = "],\"total_linhas\":" + String(lineCount) + 
                               ",\"proxima_pagina\":" + String((page < totalArquivos) ? page + 1 : 0) + 
                               ",\"pagina_anterior\":" + String((page > 1) ? page - 1 : 0) + "}";
                currentFile.close();
                inLinesArray = false;
                
                if (footer.length() <= maxLen) {
                    memcpy(buffer, footer.c_str(), footer.length());
                    return footer.length();
                }
            }
            
            return 0;
        }
    );
    
    request->send(response);
}


String lerLinhaDoArquivo(File &file) {
  String linha = file.readStringUntil('\n');
  linha.trim(); // remove \r\n
  return linha;
}

bool isValidJSONLine(const String &line) {
  return line.length() > 0; // simplificado para debug
}

void resetStreamState() {
  if (streamState.root) streamState.root.close();
  if (streamState.dateDir) streamState.dateDir.close();
  if (streamState.logFile) streamState.logFile.close();
  streamState = StreamState();
}

String getFileNameFromPath(const String& path) {
    int lastSlash = path.lastIndexOf('/');
    if (lastSlash != -1) {
        return path.substring(lastSlash + 1);
    }
    return path;
}


void enviarArquivoPorPagina(AsyncWebServerRequest *request, int page) {
    std::vector<String> arquivos;
    
    File root = LittleFS.open("/logs");
    if (root && root.isDirectory()) {
        File dir = root.openNextFile();
        while (dir) {
            if (dir.isDirectory()) {
                String dirName = dir.name();
                String fullDirPath = "/logs/" + dirName;
                
                File subDir = LittleFS.open(fullDirPath);
                if (subDir && subDir.isDirectory()) {
                    File file = subDir.openNextFile();
                    while (file) {
                        if (!file.isDirectory() && String(file.name()).endsWith(".jsonl")) {
                            String filename = getFileNameFromPath(file.name());
                            String fullFilePath = fullDirPath + "/" + filename;
                            arquivos.push_back(fullFilePath);
                        }
                        file = subDir.openNextFile();
                    }
                    subDir.close();
                }
            }
            dir = root.openNextFile();
        }
        root.close();
    }

    // DEBUG: Mostrar quantos arquivos foram encontrados e quais são
    Serial.printf("Total de arquivos encontrados: %d\n", arquivos.size());
    Serial.printf("Página solicitada: %d\n", page);
    
    for (int i = 0; i < arquivos.size(); i++) {
        Serial.printf("Arquivo %d: %s\n", i + 1, arquivos[i].c_str());
    }

    // Ordena por nome (mais recente primeiro)
    std::sort(arquivos.begin(), arquivos.end(), std::greater<String>());

    int totalArquivos = arquivos.size();

    // Verifica se a página existe
    if (page < 1 || page > totalArquivos) {
        Serial.printf("ERRO: Página %d não existe. Total: %d\n", page, totalArquivos);
        DynamicJsonDocument doc(512);
        doc["erro"] = "Página não encontrada";
        doc["total_arquivos"] = totalArquivos;
        doc["paginas_disponiveis"] = (totalArquivos > 0) ? "1 a " + String(totalArquivos) : "Nenhum arquivo";
        
        String response;
        serializeJson(doc, response);
        request->send(404, "application/json", response);
        return;
    }

    // Pega o arquivo correspondente à página
    String arquivo = arquivos[page - 1];
    Serial.printf("Enviando arquivo da página %d: %s\n", page, arquivo.c_str());
    
    enviarArquivoInteiro(request, arquivo, page, totalArquivos);
}
void setupWiFi(DeviceController& meuDevice) {
    Serial.println("Configurando modo Access Point (AP)...");

    // Inicializa o Access Point
    WiFi.softAP(ssid, password);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);



    // Endpoint para configuração do hub
    server.on("/config", HTTP_GET, [&meuDevice](AsyncWebServerRequest *request){
        // Checa se DeviceController está pronto
        if (!meuDevice.isReady()) {
            request->send(503, "application/json", R"({"error":"DeviceController não pronto"})");
            return;
        }

        DynamicJsonDocument doc(2048);

        // Informações do Hub
        JsonObject data = doc.createNestedObject("data");
        auto hubDetails = HubConfig::getInstance().getDetails();
        data["hub_id"] = hubDetails.id;
        data["hub_name"] = hubDetails.name;
        data["latitude"] = hubDetails.latitude;
        data["longitude"] = hubDetails.longitude;

        // Intervalo mínimo de amostragem
        data["min_sampling_interval_ms"] = meuDevice.getMinSamplingInterval();

        // Sensores
        JsonArray sensorsArray = doc.createNestedArray("sensors");
        for (Sensor* s : meuDevice.getSensors()) {
            if (!s) continue;

            s->toConfigJson(sensorsArray);
            /*
            JsonObject sensorJson = sensorsArray.createNestedObject();
            sensorJson["id"] = s->getSensorId();
            sensorJson["type"] = s->getSensorType();
            sensorJson["unit"] = s->getUnit();
            sensorJson["valor_critico_max"] = s->get
        */
            }

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });


#define DEBUG_SKIP_VALIDATION 0


server.on("/limpar_historico", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("🗑️ Recebido comando para limpar histórico");
    
    deleteLogFiles();
    
    // Alternativa: enviar texto plano com headers explícitos
    request->send(200, "text/plain", "OK");
    
    Serial.println("✅ Resposta de limpeza enviada");
});



server.on("/historico", HTTP_GET, [](AsyncWebServerRequest *request){
    int page = 1;
    
    if (request->hasParam("page")) {
        page = request->getParam("page")->value().toInt();
    }

    Serial.printf("Requisição para /historico. Página: %d\n", page);

    // Envia o arquivo correspondente à página
    enviarArquivoPorPagina(request, page);
});


/*
server.on("/historico", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("🎯 /historico chamado");
    
    AsyncWebServerResponse *response = request->beginChunkedResponse("application/json", [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
      size_t bytesWritten = 0;

      // RESET no início de cada requisição
      if (index == 0) {
        Serial.println("🔄 NOVO STREAM - RESET COMPLETO");
        if (streamState.root) streamState.root.close();
        streamState.root = LittleFS.open(LOG_DIR);
        streamState.dateDir = File();
        streamState.logFile = File();
        streamState.finished = false;
        streamState.totalLines = 0;
        streamState.pageCount = 0;

        if (!streamState.root || !streamState.root.isDirectory()) {
          Serial.println("❌ LOG_DIR não encontrado");
          memcpy(buffer, "[]", 2);
          streamState.finished = true;
          return 2;
        }
      }

      if (streamState.finished) return 0;

      // Abre array JSON
      if (index == 0) buffer[bytesWritten++] = '[';

      int linesThisPage = 0;

      while (!streamState.finished && bytesWritten < maxLen - 50 && linesThisPage < LINHAS_POR_PAGINA) {
        // Próximo diretório
        if (!streamState.dateDir) {
          streamState.dateDir = streamState.root.openNextFile();
          if (!streamState.dateDir) { 
            streamState.finished = true; 
            break; 
          }
          if (!streamState.dateDir.isDirectory()) {
            streamState.dateDir.close();
            streamState.dateDir = File();
            continue;
          }
          Serial.printf("📁 Abrindo diretório: %s\n", streamState.dateDir.name());
        }

        // Próximo arquivo
        if (!streamState.logFile) {
          streamState.logFile = streamState.dateDir.openNextFile();
          if (!streamState.logFile) {
            streamState.dateDir.close();
            streamState.dateDir = File();
            continue;
          }
          Serial.printf("📄 Abrindo arquivo: %s\n", streamState.logFile.name());
        }

        // Lê linhas do arquivo
        if (streamState.logFile && streamState.logFile.available()) {
          String line = streamState.logFile.readStringUntil('\n');
          line.trim();
          if (!isValidJSONLine(line)) continue;

          Serial.printf("📤 Linha: %s\n", line.c_str()); // print de debug da linha

          if (streamState.totalLines > 0 || linesThisPage > 0) buffer[bytesWritten++] = ','; // separador JSON
          size_t lineLen = line.length();
          memcpy(buffer + bytesWritten, line.c_str(), lineLen);
          bytesWritten += lineLen;

          streamState.totalLines++;
          linesThisPage++;

          // Se completar a página, retorna o chunk
          if (linesThisPage >= LINHAS_POR_PAGINA) {
            streamState.pageCount++;
            Serial.printf("📦 Página %d enviada | Total linhas até agora: %d\n", streamState.pageCount, streamState.totalLines);
            return bytesWritten;
          }

        } else {
          streamState.logFile.close();
          streamState.logFile = File();
        }
      }

      // Fecha array JSON se finalizado
      if (streamState.finished) {
        buffer[bytesWritten++] = ']';
        streamState.pageCount++;
        Serial.printf("✅ Stream finalizado | Total páginas: %d | Total linhas: %d\n", streamState.pageCount, streamState.totalLines);
      }

      return bytesWritten;
    });

    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });




*/



server.on("/info/info", HTTP_GET, [](AsyncWebServerRequest *request){
    DynamicJsonDocument doc(2048);
    JsonArray arquivos = doc.to<JsonArray>();
    int totalArquivos = 0;

    File root = LittleFS.open("/logs");
    if (root && root.isDirectory()) {
        File dir = root.openNextFile();
        while (dir) {
            if (dir.isDirectory()) {
                String dirName = dir.name();
                String fullDirPath = "/logs/" + dirName;
                
                File subDir = LittleFS.open(fullDirPath);
                if (subDir && subDir.isDirectory()) {
                    File file = subDir.openNextFile();
                    while (file) {
                        if (!file.isDirectory() && String(file.name()).endsWith(".jsonl")) {
                            String filename = getFileNameFromPath(file.name());
                            String fullFilePath = fullDirPath + "/" + filename;
                            
                            JsonObject arquivoInfo = arquivos.createNestedObject();
                            arquivoInfo["pagina"] = ++totalArquivos;
                            arquivoInfo["nome"] = filename;
                            arquivoInfo["caminho"] = fullFilePath;
                            arquivoInfo["tamanho"] = file.size();
                            arquivoInfo["modificado"] = file.getLastWrite();
                        }
                        file = subDir.openNextFile();
                    }
                    subDir.close();
                }
            }
            dir = root.openNextFile();
        }
        root.close();
    }

    doc["total_arquivos"] = totalArquivos;
    doc["paginas_disponiveis"] = (totalArquivos > 0) ? "1 a " + String(totalArquivos) : "Nenhum arquivo";

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
});







    

    server.on("/dados", HTTP_GET, [&meuDevice](AsyncWebServerRequest *request){
    // Usamos DynamicJsonDocument para flexibilidade com o número de sensores
        DynamicJsonDocument doc(1024);
        // O elemento raiz agora é um Array JSON
        JsonArray sensorsArray = doc.to<JsonArray>();
        
        const auto& sensors = meuDevice.getSensors();
        
        // Itera sobre cada objeto de sensor
        for (const auto sensor : sensors) {
            if (sensor) {
                // Para cada sensor, cria um novo objeto JSON no array
                JsonObject sensorObj = sensorsArray.createNestedObject();
                sensor->readNow();
                // Chama readNow() para fazer uma leitura fresca e imediata
                
                // Preenche o objeto JSON com todos os dados relevantes
                sensorObj["sensorId"] = sensor->getSensorId();
                sensorObj["value"] = sensor->getLastValue();
                //sensorObj["unit"] = sensor->getUnit(); // Assumindo que a classe BaseSensor tem o método getUnit()
        }
    }

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });


    server.begin();
    Serial.println("Servidor Web iniciado.");
}

