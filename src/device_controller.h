#ifndef DEVICE_CONTROLLER_H
#define DEVICE_CONTROLLER_H

// Incluímos a classe base e TODAS as classes filhas que o dispositivo pode ter.
#include "sensors/BaseSensor.h"
#include "sensors/TemperatureSensor.h"
#include "sensors/TdsSensor.h"
#include "sensors/FlowSensor.h"
#include "sensors/VolumeSensor.h"
#include "sensors/PressureSensor.h"
#include <vector>

// #include "TemperatureSensor.h" // Adicione aqui os outros .h dos seus sensores

// Uma struct simples para a configuração de BLE, para manter o código limpo.
struct HubBleConfig {
    String service_uuid;
    String data_characteristic_uuid;
};

class DeviceController {
public:
    DeviceController();
    ~DeviceController();

    /**
     * @brief Inicializa o dispositivo, carregando os 5 arquivos de config
     * e criando cada um dos sensores específicos.
     */
    bool init();

    // --- PONTEIROS PÚBLICOS E NOMEADOS PARA CADA SENSOR ---
    // O acesso será direto: ex: meuDevice.sensor_solo->getValue()
    TdsSensor* sensor_tds;
    VolumeSensor* sensor_volume;
    FlowSensor* sensor_flow;
    TemperatureSensor* sensor_temperature;
    PressureSensor* sensor_pressure;

    // TemperatureSensor* sensor_temperatura; // Exemplo para outro sensor
    // PhSensor* sensor_ph;          // Exemplo para outro sensor
    // ... adicione os ponteiros para os seus 5 tipos de sensores aqui

    // Getter para a configuração de BLE
    const HubBleConfig& getBleConfig() const;
    bool isReady() const;
    const std::vector<Sensor*>& getSensors() const;
    long getMinSamplingInterval();

private:
    std::vector<Sensor*> _sensors;
    HubBleConfig _bleConfig;
    bool _isReady;
    long _realtimeNotifyIntervalMs;
};

#endif // DEVICE_CONTROLLER_H