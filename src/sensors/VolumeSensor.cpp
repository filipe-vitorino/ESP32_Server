#include "VolumeSensor.h"
#include <ArduinoJson.h>
#include <Arduino.h>

VolumeSensor::VolumeSensor(FlowSensor* flowSensor)
    : _flowSensor(flowSensor), _accumulatedVolume(0.0f), _rangeMin(0.0f), _rangeMax(1000.0f) {}

// ----------------------- Calibração -----------------------
void VolumeSensor::_configureCalibration(const JsonVariant& calibrationConfig) {
    _rangeMin = calibrationConfig["valid_range"]["min"] | 6.0f;
    _rangeMax = calibrationConfig["valid_range"]["max"] | 600.0f;

    Serial.printf("  -> VolumeSensor (ID: %s) calibrado: range=[%.1f, %.1f]\n",
                  this->_sensor_id.c_str(), _rangeMin, _rangeMax);
}

// ----------------------- Raw -----------------------
int VolumeSensor::getRaw() {
    // Retorna o volume acumulado em mL como inteiro
    return (int)(_accumulatedVolume * 1000.0f);
}

// ----------------------- Valor calibrado -----------------------
float VolumeSensor::getValue(int rawValue) {
    // Retorna volume acumulado em litros
    return _accumulatedVolume;
}

// ----------------------- Update -----------------------
void VolumeSensor::update(){
    // Chama update do FlowSensor implicitamente pelo getValue
    time_t current_ts = rtcService.getTimestamp(); 


    unsigned long currentMillis = millis();
    if (currentMillis - _lastSampleMillis >= (_sampling_period_sec * 1000L)) {
        _lastSampleMillis = currentMillis;
        int pulses = _flowSensor->getRaw();
        float flowLPerMin = _flowSensor->getValue(pulses);

        // Converte fluxo instantâneo para volume acumulado no período de amostragem
        // volume = fluxo (L/min) * tempo (min)
        float deltaTimeMin = _sampling_period_sec / 60.0f; 
        _accumulatedVolume += flowLPerMin * deltaTimeMin;

        // Limita ao intervalo definido
        if (_accumulatedVolume < _rangeMin) _accumulatedVolume = _rangeMin;
        if (_accumulatedVolume > _rangeMax) _accumulatedVolume = _rangeMax;

        // Notifica e registra leitura
        notifySensorValue(_sensor_id, _accumulatedVolume, _unit);
        logSensorReading(current_ts,_sensor_id, _sensor_type,_unit, 0, _accumulatedVolume); 
         Serial.println("Hora de salvar. 2");

    }
   
}
