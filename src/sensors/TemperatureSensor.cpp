#include "TemperatureSensor.h"

TemperatureSensor::TemperatureSensor(uint8_t pin)
    : _pin(pin), _oneWire(nullptr), _sensors(nullptr), _index(0),
      _rangeMin(-50.0f), _rangeMax(125.0f)
{
    // O pino é configurado dentro do OneWire/Dallas
}

// ----------------------- Calibração -----------------------
void TemperatureSensor::_configureCalibration(const JsonVariant& calibrationConfig) {
    _unit = calibrationConfig["unit"] | "°C";
    _index = calibrationConfig["index"] | 0;

    _rangeMin = calibrationConfig["valid_range"]["min"] | -50.0f;
    _rangeMax = calibrationConfig["valid_range"]["max"] | 125.0f;

    // Inicializa biblioteca DallasTemperature
    _oneWire = new OneWire(_pin);
    _sensors = new DallasTemperature(_oneWire);
    _sensors->begin();

    Serial.printf(" Temperatura -> Sensor DS18B20 (ID: %s) configurado: unidade=%s, índice=%d, range=[%.1f, %.1f]\n",
                  _sensor_id.c_str(), _unit.c_str(), _index, _rangeMin, _rangeMax);
}

// ----------------------- Raw -----------------------
int TemperatureSensor::getRaw() {
    // DS18B20 não tem raw analógico, retornamos 0 apenas para manter a assinatura
    return 0;
}

// ----------------------- Valor calibrado -----------------------
float TemperatureSensor::getValue(int rawValue) {
    if (!_sensors) return 0.0f;

    _sensors->requestTemperatures();
    float temp = _sensors->getTempCByIndex(_index);

    // Limita aos valores mínimos/máximos definidos
    if (temp < _rangeMin) temp = _rangeMin;
    if (temp > _rangeMax) temp = _rangeMax;
    //temp = random(100, 300)/10;
    return temp;
}
