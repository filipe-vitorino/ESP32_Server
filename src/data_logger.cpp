#include "data_logger.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <sys/time.h>
#include <vector>
// --- VARIÁVEIS DE ESTADO PARA O STREAMING ---
static std::vector<String> _streamFilePaths;
static int _currentStreamFileIndex = -1;
static File _currentStreamFile;
static bool _isFirstChunk = true;

const char* LOG_DIR = "/logs";
#define LOG_LINES_PER_BLOCK 50
File logFileBLE; // Handle de ficheiro para leitura sequencial do BLE

// Inicializa LittleFS e cria diretório raiz
void setupDataLogger() {
    if (!LittleFS.begin(true)) { // true -> formata se necessário
        Serial.println("Falha ao montar o LittleFS!");
        return;
    }
    Serial.println("LittleFS montado com sucesso.");

    if (!LittleFS.exists(LOG_DIR)) {
        if (LittleFS.mkdir(LOG_DIR)) {
            Serial.println("Diretório de logs principal '/logs' criado.");
        }
    }
}

// Salva leitura de sensor em subpasta diária
void logSensorReading(time_t now, const String& sensorId, const String& sensorType, const String& unit, int rawValue, float calibratedValue) {

    // Verifica hora válida
    if (now < 1704067200) {
        Serial.println("-> Hora inválida, log não será salvo.");
        return;
    }

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Cria caminho do diretório diário
    char dirPath[32];
    sprintf(dirPath, "%s/%d_%02d_%02d", LOG_DIR, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);

    if (LittleFS.exists(dirPath)){
      //Serial.printf("JA EXISTE: %s\n", dirPath);
    }
    // Cria diretório diário se não existir
    if (!LittleFS.exists(dirPath)) {
        if (LittleFS.mkdir(dirPath)) {
           // Serial.printf("Novo diretório diário criado: %s\n", dirPath);
        } else {
           // Serial.printf("Falha ao criar diretório diário: %s\n", dirPath);
            return;
        }
    }

    // Caminho completo do arquivo do sensor
    String filePath = String(dirPath) + "/" + sensorId + ".jsonl";

    File file = LittleFS.open(filePath, FILE_APPEND);
    if (!file) {
        Serial.printf("Falha ao abrir o arquivo de log: %s\n", filePath.c_str());
        return;
    }

    // Cria objeto JSON
    StaticJsonDocument<256> doc;
    char isoTimestamp[21];
    strftime(isoTimestamp, sizeof(isoTimestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

    doc["ts"] = isoTimestamp;
    doc["sensorId"] = sensorId;
    doc["sensorType"] = sensorType;
    doc["raw"] = rawValue;
    doc["value"] = serialized(String(calibratedValue, 2));
    doc["unit"] = unit;

    if (serializeJson(doc, file)) {
        file.println();
        //Serial.printf("Registro salvo para '%s'\n", sensorId.c_str());
    } else {
        Serial.println("Falha ao escrever JSON no arquivo.");
    }

    file.close();
}

// Ajusta o relógio do ESP32
void setSystemTime(time_t epochTime) {
    struct timeval tv;
    tv.tv_sec = epochTime;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    struct tm timeinfo;
    if(getLocalTime(&timeinfo)){
        Serial.print("✅ ESP32: Hora do sistema acertada pelo app para: ");
        Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    }
}

// Em src/data_logger.cpp

void deleteLogFiles() {
    int totalCount = 0;
    File root = LittleFS.open(LOG_DIR);
    if (!root || !root.isDirectory()) {
        Serial.println("❌ Falha ao abrir LOG_DIR ou não é diretório.");
        return;
    }

    File dir = root.openNextFile();
    while (dir) {
        if (dir.isDirectory()) {
            Serial.printf("📁 Diretório: %s\n", dir.name());

            File f = dir.openNextFile();
            while (f) {
                if (!f.isDirectory()) {
                    String filePath = String(LOG_DIR) + "/" + String(dir.name()) + "/" + f.name();
                    Serial.printf("   🗑️ Deletando: %s\n", filePath.c_str());
                    f.close();
                    if (LittleFS.remove(filePath)) {
                        Serial.println("   ✅ Arquivo deletado com sucesso.");
                    } else {
                        Serial.println("   ❌ Falha ao deletar arquivo.");
                    }

                    totalCount++;
                }
                
                f = dir.openNextFile();
            }

            // Após deletar arquivos, pode remover o diretório também (opcional)
            String dirPath = String(LOG_DIR) + "/" + String(dir.name());
            if (LittleFS.rmdir(dirPath)) {
                Serial.printf("📂 Diretório %s removido.\n", dirPath.c_str());
            } else {
                Serial.printf("⚠️ Não foi possível remover diretório %s.\n", dirPath.c_str());
            }
        }
        dir.close();
        dir = root.openNextFile();
    }
    root.close();

    Serial.printf("ℹ️ Total de arquivos deletados: %d\n", totalCount);
}


// Conta total de registros em todos os arquivos de log
int getTotalRecordsInAllFiles() {
    int totalCount = 0;
    File root = LittleFS.open(LOG_DIR);
    if (!root || !root.isDirectory()) {
        Serial.println("❌ Falha ao abrir LOG_DIR ou não é diretório.");
        return 0;
    }

    File dir = root.openNextFile();
    while(dir) {
        if(dir.isDirectory()) {
            Serial.printf("📁 Diretório: %s\n", dir.name());

            File f = dir.openNextFile();
            while(f) {
                if(!f.isDirectory()) {
                    Serial.printf("   📄 Arquivo: %s\n", f.name());

                    while(f.available()) {
                        String line = f.readStringUntil('\n');
                        //Serial.printf("      📝 Linha lida: %s\n", line.c_str());
                        if(line.length() > 2) totalCount++;
                    }
                }
                f.close();
                f = dir.openNextFile();
            }
        }
        dir.close();
        dir = root.openNextFile();
    }
    root.close();
    Serial.printf("ℹ️ Total de registros encontrados: %d\n", totalCount);
    return totalCount;
}

// Funções auxiliares para BLE
bool openLogFileForRead(const String& filePath) {
    logFileBLE = LittleFS.open(filePath, "r");
    return logFileBLE;
}

String readNextLogEntry() {
    return (logFileBLE && logFileBLE.available()) ? logFileBLE.readStringUntil('\n') : "";
}

void closeLogFile() {
    if (logFileBLE) logFileBLE.close();
}

/// Obtém uma lista com o caminho absoluto de todos os ficheiros de log.
std::vector<String> getAllLogFilePaths() {
    std::vector<String> filePaths;
    File root = LittleFS.open(LOG_DIR, "r");
    if (!root || !root.isDirectory()) {
        return filePaths;
    }

    File dateDir = root.openNextFile();
    while(dateDir){
        if(dateDir.isDirectory()){
            File logDir = LittleFS.open(dateDir.name());
            File logFile = logDir.openNextFile();
            while(logFile){
                if(!logFile.isDirectory() && String(logFile.name()).endsWith(".jsonl")){
                    filePaths.push_back(String(logFile.name()));
                }
                logFile.close();
                logFile = logDir.openNextFile();
            }
            logDir.close();
        }
        dateDir.close();
        dateDir = root.openNextFile();
    }
    root.close();
    return filePaths;
}

void streamFileJson(AsyncResponseStream* response, File &file, const String &filename) {
    response->write("{\"file\":\"", 9);
    response->write((const uint8_t*)filename.c_str(), filename.length());
    response->write("\",\"content\":\"", 12);

    const size_t bufSize = 256;
    char buf[bufSize];
    while(file.available()) {
        size_t len = file.readBytes(buf, bufSize);
        for(size_t i = 0; i < len; i++) {
            char c = buf[i];
            if(c == '"' || c == '\\') response->write('\\'); // escape JSON
            response->write(c);
        }
    }

    response->write("\"}\n", 3);
}

void prepareLogStream() {
    Serial.println("Preparando stream de logs...");
    _streamFilePaths.clear();
    _currentStreamFileIndex = -1;
    _isFirstChunk = true;

    File root = LittleFS.open(LOG_DIR);
    if (!root || !root.isDirectory()) return;

    File dateDir = root.openNextFile();
    while(dateDir){
        if(dateDir.isDirectory()){
            File logDir = LittleFS.open(dateDir.name());
            File logFile = logDir.openNextFile();
            while(logFile){
                if(!logFile.isDirectory() && String(logFile.name()).endsWith(".jsonl")){
                    _streamFilePaths.push_back(String(logFile.name()));
                }
                logFile.close();
                logFile = logDir.openNextFile();
            }
            logDir.close();
        }
        dateDir.close();
        dateDir = root.openNextFile();
    }
    root.close();
    Serial.printf("Encontrados %d ficheiros de log para o stream.\n", _streamFilePaths.size());
}

size_t readLogStreamChunk(uint8_t *buffer, size_t maxLen) {
    size_t bytesWritten = 0;

    // Se for o primeiro pedaço de todos, envia o '[' de abertura do array JSON
    if (_isFirstChunk) {
        buffer[bytesWritten++] = '[';
        _isFirstChunk = false;
    }

    while (bytesWritten < maxLen) {
        // Se não houver ficheiro aberto, tenta abrir o próximo da lista
        if (!_currentStreamFile) {
            _currentStreamFileIndex++;
            if (_currentStreamFileIndex >= _streamFilePaths.size()) {
                // Não há mais ficheiros, termina o stream
                break; 
            }
            _currentStreamFile = LittleFS.open(_streamFilePaths[_currentStreamFileIndex], "r");
            if (!_currentStreamFile) continue;
        }

        // Se o ficheiro atual não tiver mais nada, fecha-o e passa para o próximo
        if (!_currentStreamFile.available()) {
            _currentStreamFile.close();
            continue;
        }

        // Lê a próxima linha do ficheiro
        String line = _currentStreamFile.readStringUntil('\n');
        if (line.length() > 2) {
            // Adiciona a vírgula se não for o primeiro elemento do array JSON
            if (bytesWritten > 1) { // Maior que 1 para não adicionar vírgula depois do '[' inicial
                if (bytesWritten + 1 < maxLen) {
                    buffer[bytesWritten++] = ',';
                } else break;
            }

            // Copia a linha para o buffer, se houver espaço
            if (bytesWritten + line.length() < maxLen) {
                memcpy(buffer + bytesWritten, line.c_str(), line.length());
                bytesWritten += line.length();
            } else {
                // Se a linha não couber, volta a colocar no ficheiro e termina este chunk
                _currentStreamFile.seek(_currentStreamFile.position() - line.length() - 1);
                break;
            }
        }
    }

    // Se terminámos todos os ficheiros e ainda há espaço no buffer, envia o ']' de fecho
    if (_currentStreamFileIndex >= _streamFilePaths.size() && bytesWritten + 1 < maxLen) {
        buffer[bytesWritten++] = ']';
        // Marca o índice como "superado" para indicar que terminámos
        _currentStreamFileIndex++; 
    }

    // Se terminámos e não escrevemos nada (nem o '['), retorna 0
    if (bytesWritten == 0 && _currentStreamFileIndex > _streamFilePaths.size()) {
        return 0;
    }

    return bytesWritten;
}