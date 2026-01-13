#include "BaseSensor.h"
#include <Arduino.h>


// Implementação do Construtor
Sensor::Sensor() {
    _lastSampleMillis = 0;
    _lastValue = 0;

}

// Implementação do Destrutor
Sensor::~Sensor() {}

// Implementação da configuração dos dados comuns
void Sensor::configure(const JsonVariant& configJson) {
    _sensor_type = configJson["sensor_type"].as<String>();
    _sensor_id = configJson["sensor_id"].as<String>();
    _pin = configJson["pin"] | -1;
    _unit = configJson["unit"].as<String>();
    _sampling_period_sec = configJson["sampling_period_sec"] | 900;
    _ble_characteristic_uuid = configJson["ble"]["characteristic_uuid"].as<String>();
    _valorCriticoMax = configJson["valor_critico"]["max"];
    _valorCriticoMin = configJson["valor_critico"]["min"];

    // Inicializa o temporizador para permitir a primeira leitura imediatamente
    _lastSampleMillis = 0 - (_sampling_period_sec * 1000L);
    
    serializeJsonPretty(configJson["calibration"], Serial);
    Serial.println();
    // Chama o método virtual para que a classe filha configure a sua calibração específica
    _configureCalibration(configJson["calibration"]);
}

// Implementação do método de ciclo de vida principal
void Sensor::update() {
    //time_t current_ts = 1704067201; 
   
    time_t current_ts = rtcService.getTimestamp(); 
    int rawValue = getRaw();
    float calibratedValue = getValue(rawValue);
        
    _lastValue = calibratedValue;
    unsigned long currentMillis = millis();
    if (currentMillis - _lastSampleMillis >= (_sampling_period_sec * 1000L)) {
        _lastSampleMillis = currentMillis;
        logSensorReading(current_ts, _sensor_id, _sensor_type, _unit, rawValue, calibratedValue);
        Serial.println("🧾 Dado salvo no log.");
    }
    // 📡 2. Controle da notificação BLE (a cada 2 segundos fixos)
    if (currentMillis - _lastNotifyMillis >= 2000) {  // 2000 ms = 2 segundos
        _lastNotifyMillis = currentMillis;
        // Envia o último valor conhecido (_lastValue)
        notifySensorValue(_sensor_id, _lastValue, _unit);
        Serial.println("📡 Notificação BLE enviada.");
    }
}

// Implementação dos Getters
String Sensor::getSensorId() const {
    return _sensor_id;
}

String Sensor::getCharacteristicUuid() const {
    return _ble_characteristic_uuid;
}

String Sensor::getUnit() const
{
    return _unit;
}
String Sensor::getSensorType() const
{
    return _sensor_type;
}

void Sensor::readNow(){
    int rawValue = getRaw();
    float calibratedValue = getValue(rawValue);
    _lastValue = calibratedValue;
}
void Sensor::notify() {
    if (!_ble_characteristic_uuid) return; // characteristic é o BLECharacteristic do sensor
    int rawValue = getRaw();
    float calibratedValue = getValue(rawValue);
    notifySensorValue(_sensor_id, calibratedValue, _unit);
  }

long Sensor::getSamplingPeriod() const{
    return _sampling_period_sec;
}

float Sensor::getLastValue() const{
    return _lastValue;
}