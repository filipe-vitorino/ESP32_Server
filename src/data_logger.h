#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <Arduino.h>
#include <FS.h>
#include "time.h"
#include <ESPAsyncWebServer.h>
extern const char* LOG_DIR;
// Funções de configuração e escrita
void setupDataLogger();
void setSystemTime(time_t epochTime);
void deleteLogFiles(); // Apaga todos os logs
void logSensorReading(time_t timestamp, const String& sensorId, const String& unit, int rawValue, float calibratedValue);
// Funções de leitura para o processo de sincronização BLE
int getTotalRecordsInAllFiles();
bool openLogFileForRead(const String& filePath);
String readNextLogEntry();
void closeLogFile();
void streamAllLogFiles(AsyncResponseStream *response);
std::vector<String> getAllLogFilePaths();
size_t readLogStreamChunk(uint8_t *buffer, size_t maxLen);
void prepareLogStream();

#endif