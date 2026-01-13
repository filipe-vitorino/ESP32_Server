#ifndef BASE_SENSOR_H
#define BASE_SENSOR_H

#include <ArduinoJson.h>
#include "../rtc_service.h"
// Forward declarations to avoid circular dependencies
void logSensorReading(time_t timestamp, const String& sensorId,const String& sensorType, const String& unit, int rawValue, float calibratedValue);
void notifySensorValue(const String& sensorId, float value, const String& unit);

class Sensor {
public:
    // Construtor e Destrutor (apenas declarações)
    Sensor();
    virtual ~Sensor();

    // Método de configuração que as classes filhas usarão
    void configure(const JsonVariant& configJson);

    // O coração do sensor, chamado pelo loop principal
    virtual void update();

    virtual int getRaw() = 0;
    virtual float getValue(int rawValue) = 0;
    
    void notify();
    // Getters para os dados
    String getSensorId() const;
    String getCharacteristicUuid() const;
    String getUnit() const;
    String getSensorType() const;
    void readNow();
    long getSamplingPeriod() const;
    float getLastValue() const;
    virtual void toConfigJson(JsonArray& array) {
        JsonObject obj = array.createNestedObject();
        obj["sensor_id"] = _sensor_id;
        obj["sensorType"] = _sensor_type;
        obj["unit"] = _unit;
        obj["uuid_c"] = _ble_characteristic_uuid;

        JsonObject crit = obj.createNestedObject("valor_critico");
        crit["min"] = _valorCriticoMin;
        crit["max"] = _valorCriticoMax;
    }

protected:
    // Método de calibração que as classes filhas DEVEM implementar
    virtual void _configureCalibration(const JsonVariant& calibrationConfig) = 0;

    // Membros de dados (protegidos para que as classes filhas possam acedê-los)
    String _sensor_type;
    String _sensor_id;
    int _pin;
    String _unit;
    float _valorCriticoMin;
    float _valorCriticoMax;
    long _sampling_period_sec;
    String _ble_characteristic_uuid;
    unsigned long _lastSampleMillis;
    float _lastValue;
    void notifyBLE(float value);
    unsigned long _lastNotifyMillis = 0;
    RTCService rtcService;
};

#endif // BASE_SENSOR_H