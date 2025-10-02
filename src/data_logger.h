#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include "FS.h"
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>

void setupDataLogger();
void logFakeSensorData();

// Para o Wi-Fi
void streamLogFile(AsyncResponseStream *response);
size_t streamLogFileChunked(uint8_t *buffer, size_t maxLen, size_t index);
String getLogPage(int page, int limit);
// Para o BLE
int getTotalRecords();
bool openLogFileForRead();
String readNextLogEntry();
void closeLogFile();
void deleteLogFile();
#endif