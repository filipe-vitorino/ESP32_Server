#ifndef RTC_SERVICE_H
#define RTC_SERVICE_H

#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"

class RTCService {
public:
    RTCService();

    bool begin();  
    String getRealTime();  
    void setDateTime(uint16_t year, uint8_t month, uint8_t day,
                     uint8_t hour, uint8_t minute, uint8_t second);
    time_t getTimestamp();
    void adjustToCompileTime();

private:
    RTC_DS3231 rtc;
    bool initialized;
};

#endif
