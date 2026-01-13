#ifndef VOLUME_SENSOR_H
#define VOLUME_SENSOR_H

#include "BaseSensor.h"
#include "FlowSensor.h"

class VolumeSensor : public Sensor {
public:
    VolumeSensor(FlowSensor* flowSensor);

    int getRaw() override;
    float getValue(int rawValue) override;
    void update() override;

protected:
    void _configureCalibration(const JsonVariant& calibrationConfig) override;

private:
    FlowSensor* _flowSensor;

    float _accumulatedVolume; // em litros
    float _rangeMin;
    float _rangeMax;
};

#endif
