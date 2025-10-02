#include "data_logger.h"
#include <ArduinoJson.h>
#include "time.h"

const char* LOG_FILE = "/historico.json";
const char* TEMP_LOG_FILE = "/historico_temp.json";
const int RETENTION_DAYS = 7;
File logFileBLE;

void pruneLogFile() {
  Serial.println("Iniciando verificação de logs antigos...");
  time_t now;
  time(&now);
  time_t cutoffTimestamp = now - (RETENTION_DAYS * 24 * 60 * 60);

  File originalFile = SPIFFS.open(LOG_FILE, "r");
  if (!originalFile) return;
  File tempFile = SPIFFS.open(TEMP_LOG_FILE, "w");
  if (!tempFile) { originalFile.close(); return; }

  int recordsKept = 0, recordsDeleted = 0;
  while(originalFile.available()) {
    String line = originalFile.readStringUntil('\n');
    if (line.length() > 2) {
      StaticJsonDocument<200> doc;
      deserializeJson(doc, line);
      time_t recordTimestamp = doc["ts"];
      if (recordTimestamp >= cutoffTimestamp) {
        tempFile.println(line);
        recordsKept++;
      } else {
        recordsDeleted++;
      }
    }
  }
  originalFile.close();
  tempFile.close();

  SPIFFS.remove(LOG_FILE);
  SPIFFS.rename(TEMP_LOG_FILE, LOG_FILE);
  Serial.printf("Limpeza concluída. Mantidos: %d, Apagados: %d\n", recordsKept, recordsDeleted);
}

void setupDataLogger() {
  if (!SPIFFS.begin(true)) { Serial.println("Falha ao montar o SPIFFS!"); return; }
  Serial.println("SPIFFS montado com sucesso.");
  pruneLogFile(); 
}

void logFakeSensorData() {
  File file = SPIFFS.open(LOG_FILE, FILE_APPEND);
  if (!file) { Serial.println("Falha ao abrir arquivo de log."); return; }
  
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return;
  time_t now;
  time(&now);

  StaticJsonDocument<200> doc;
  doc["ts"] = now;
  doc["vazao"] = random(100, 200) / 10.0;
  doc["temperatura"] = random(200, 300) / 10.0;
  
  if (serializeJson(doc, file)) file.println();
  file.close();
  Serial.println("Novo registro de log salvo.");
}

size_t streamLogFileChunked(uint8_t *buffer, size_t maxLen, size_t index) {
  static File file;
  static bool first;
  static bool finished;

  // index == 0 => início da stream (nova requisição)
  if (index == 0) {
    first = true;
    finished = false;
    if (file) { file.close(); } // garante que não haja handle aberto
    extern const char* LOG_FILE;
    file = SPIFFS.open(LOG_FILE, "r");
    if (!file) {
      // ficheiro não existe -> envia array vazio "[]"
      const char *empty = "[]";
      memcpy(buffer, empty, 2);
      finished = true; // próxima chamada deve retornar 0 e encerrar definitivamente
      return 2;
    }
    // envia o '[' de abertura
    buffer[0] = '[';
    return 1;
  }

  // se já terminou (envio do ']'), faz cleanup e retorna 0 (fim)
  if (finished) {
    if (file) { file.close(); }
    finished = false;
    return 0;
  }

  // lê até encontrar a próxima linha válida (pula linhas vazias / CR)
  while (file && file.available()) {
    String line = file.readStringUntil('\n');
    if (line.length() <= 2) continue; // pula linhas vazias ou só \r

    if (!first) {
      line = "," + line; // separador entre objetos JSON
    }
    first = false;

    size_t len = line.length();
    if (len > maxLen) {
      // se por acaso a linha for maior que o buffer, truncamos (melhorar se necessário)
      len = maxLen;
    }
    memcpy(buffer, line.c_str(), len);
    return len;
  }

  // não há mais dados -> fecha JSON com ']' e marca finished para a próxima chamada devolver 0
  const char *end = "]";
  memcpy(buffer, end, 1);
  finished = true;
  return 1;
}


void streamLogFile(AsyncResponseStream *response) {
  File file = SPIFFS.open(LOG_FILE, "r");
  if (!file || !file.size()) {
    Serial.println("Ficheiro de log não encontrado ou vazio.");
    response->print("[]");
    // Não precisamos de `request->send(response)` aqui, pois o handler no wifi_handler.cpp fará isso.
    return;
  }

  Serial.println("Iniciando stream cooperativo do ficheiro de log...");
  response->print("[");
  bool first = true;

  // Cria um buffer de tamanho fixo na stack para ler cada linha
  const size_t bufferSize = 256;
  char lineBuffer[bufferSize];

  while (file.available()) {
    // Lê a próxima linha do ficheiro diretamente para o buffer, até ao '\n'
    size_t bytesRead = file.readBytesUntil('\n', lineBuffer, bufferSize - 1);
    
    if (bytesRead > 0) {
      // Adiciona o terminador nulo para tratar o buffer como uma string C válida
      lineBuffer[bytesRead] = '\0';

      // Espera até que haja espaço suficiente no buffer de envio da rede
      // para a linha que acabámos de ler.
      while(response->availableForWrite() < (bytesRead + 2)) {
        // Enquanto espera, cede tempo ao sistema
        delay(5);
      }
      
      // Agora que há espaço, envia os dados
      if (!first) {
        response->print(",");
      }
      response->print(lineBuffer);
      first = false;
    }
    
    // Cede controlo ao sistema operativo a CADA iteração, mesmo que
    // a leitura da linha seja muito rápida. Isto é crucial para o watchdog.
    delay(0); 
  }
  
  response->print("]");
  file.close();
  Serial.println("Stream do ficheiro de log concluído.");
}

String getLogPage(int page, int limit) {
  File file = SPIFFS.open(LOG_FILE, "r");
  if (!file || !file.size()) {
    return "[]"; // Retorna um array vazio se não houver ficheiro ou estiver vazio
  }

  // Calcula quantos registos precisa de saltar para chegar à página certa
  // Se page=1, não salta nenhum. Se page=2, salta 'limit' registos.
  int recordsToSkip = (page - 1) * limit;

  // Salta as linhas (registos) necessárias
  for (int i = 0; i < recordsToSkip; i++) {
    if (!file.available()) break; // Para se o ficheiro acabar antes
    file.readStringUntil('\n');
  }

  String output = "[";
  bool first = true;
  for (int i = 0; i < limit; i++) {
    if (!file.available()) break; // Para se o ficheiro acabar
    String line = file.readStringUntil('\n');
    if (line.length() > 2) { // Garante que a linha não está vazia
      if (!first) {
        output += ",";
      }
      output += line;
      first = false;
    }
  }
  output += "]";
  file.close();
  return output;
}




int getTotalRecords() {
    File file = SPIFFS.open(LOG_FILE, "r");
    if (!file) return 0;
    int count = 0;
    while(file.readStringUntil('\n').length() > 0) count++;
    file.close();
    return count;
}

bool openLogFileForRead() { logFileBLE = SPIFFS.open(LOG_FILE, "r"); return logFileBLE; }
String readNextLogEntry() { return (logFileBLE && logFileBLE.available()) ? logFileBLE.readStringUntil('\n') : ""; }
void closeLogFile() { if (logFileBLE) logFileBLE.close(); }

void deleteLogFile() {
  if (SPIFFS.remove(LOG_FILE)) {
    Serial.println("Ficheiro de log apagado com sucesso por comando do app.");
  } else {
    Serial.println("Falha ao apagar o ficheiro de log.");
  }
}
