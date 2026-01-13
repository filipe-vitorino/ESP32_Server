#ifndef FLOW_SENSOR_H
#define FLOW_SENSOR_H

#include "BaseSensor.h"

class FlowSensor : public Sensor {
public:
    FlowSensor(uint8_t pin);

    int getRaw() override;
    float getValue(int rawValue) override;

protected:
    void _configureCalibration(const JsonVariant& calibrationConfig) override;

private:
    uint8_t _pin;
    volatile unsigned long _pulseCount;

    float _factor;      // pulsos -> L/min
    float _rangeMin;    // mínimo valor de vazão
    float _rangeMax;    // máximo valor de vazão

    static void _pulseISR(); // rotina de interrupção
};

#endif
