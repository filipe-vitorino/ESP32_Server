#include "FlowSensor.h"
#include <Arduino.h>
#include <ArduinoJson.h>

FlowSensor* _instanceForISR = nullptr; // ponteiro para chamar ISR

// ----------------------- Construtor -----------------------
FlowSensor::FlowSensor(uint8_t pin) 
    : _pin(pin), _pulseCount(0), _factor(1.0), _rangeMin(0.0), _rangeMax(100.0) 
{
    pinMode(_pin, INPUT_PULLUP);
    _instanceForISR = this;
    attachInterrupt(digitalPinToInterrupt(_pin), _pulseISR, RISING);
}

// ----------------------- ISR -----------------------
void FlowSensor::_pulseISR() {
    if (_instanceForISR) {
        _instanceForISR->_pulseCount++;
    }
}

// ----------------------- Calibração -----------------------
void FlowSensor::_configureCalibration(const JsonVariant& calibrationConfig) {
    _factor = calibrationConfig["factor"] | 3.0;
    _rangeMin = calibrationConfig["valid_range"]["min"] | 11.0;
    _rangeMax = calibrationConfig["valid_range"]["max"] | 111.0;

    Serial.printf("  -> FlowSensor (ID: %s) calibrado: factor=%.3f, range=[%.1f, %.1f]\n",
                  _sensor_id.c_str(), _factor, _rangeMin, _rangeMax);
}

// ----------------------- Raw -----------------------
int FlowSensor::getRaw() {
    // Retorna pulsos acumulados e zera o contador
    noInterrupts();
    int pulses = _pulseCount;
    _pulseCount = 0;
    interrupts();
    return pulses;
}

// ----------------------- Valor calibrado -----------------------
float FlowSensor::getValue(int rawValue) {
    float flow = rawValue * _factor;

    // Limita ao intervalo
    if (flow < _rangeMin) flow = _rangeMin;
    if (flow > _rangeMax) flow = _rangeMax;
    //flow = random(100, 300)/10;
    return flow;
}
