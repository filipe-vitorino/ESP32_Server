#include "rtc_service.h"

RTCService::RTCService() {
    initialized = false;
}

void RTCService::adjustToCompileTime() {
    if (!initialized) return;

    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

    Serial.println("[RTC] Ajustado automaticamente para data/hora da compilação.");
}

bool RTCService::begin() {
    Wire.begin();

    if (!rtc.begin()) {
        Serial.println("[RTC] Erro: não foi possível iniciar o RTC.");
        initialized = false;
        return false;
    }

    initialized = true;
    Serial.println("[RTC] RTC iniciado com sucesso.");
    return true;
}

String RTCService::getRealTime() {
    if (!initialized) {
        return "RTC_NOT_INITIALIZED";
    }

    DateTime now = rtc.now();

    char buffer[25];
    sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d",
            now.day(), now.month(), now.year(),
            now.hour(), now.minute(), now.second());

    return String(buffer);
}

void RTCService::setDateTime(uint16_t year, uint8_t month, uint8_t day,
                             uint8_t hour, uint8_t minute, uint8_t second) {
    if (!initialized) return;

    rtc.adjust(DateTime(year, month, day, hour, minute, second));

    Serial.print("[RTC] Data e hora ajustadas para: ");
    Serial.print(day); Serial.print("/");
    Serial.print(month); Serial.print("/");
    Serial.print(year); Serial.print(" ");
    Serial.print(hour); Serial.print(":");
    Serial.print(minute); Serial.print(":");
    Serial.println(second);
}

time_t RTCService::getTimestamp() {
    if (!initialized) return 0;

    DateTime now = rtc.now();
    return now.unixtime();
}