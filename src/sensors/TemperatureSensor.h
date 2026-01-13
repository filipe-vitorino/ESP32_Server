#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include "BaseSensor.h"
#include <OneWire.h>
#include <DallasTemperature.h>

class TemperatureSensor : public Sensor {
public:
    TemperatureSensor(uint8_t pin);

    float getValue(int rawValue) override;
    int getRaw() override;

protected:
    void _configureCalibration(const JsonVariant& calibrationConfig) override;

private:
    uint8_t _pin;

    OneWire* _oneWire;
    DallasTemperature* _sensors;
    int _index;

    float _rangeMin;
    float _rangeMax;
};

#endif
