#include "device_controller.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

DeviceController::DeviceController() :
    sensor_pressure(nullptr),
    sensor_flow(nullptr),
    sensor_tds(nullptr),
    sensor_temperature(nullptr),
    sensor_volume(nullptr),
    _isReady(false)
{}

DeviceController::~DeviceController() {
    for (auto sensor : _sensors) {
        delete sensor;
    }
    _sensors.clear();
}

bool DeviceController::init() {
    // Monta o LittleFS, formata se estiver corrompido
    if (!LittleFS.begin(true)) {
        Serial.println("Falha ao montar o LittleFS.");
        return false;
    }

    const char* configFiles[] = {
        "/vazao.json", "/volume.json", "/temperatura.json", "/pressao.json", "/tds.json"
    };

    // --- FASE 1: CRIAR SENSORES INDEPENDENTES ---
    Serial.println("--- Fase 1: Inicializando sensores independentes ---");
    for (const char* filename : configFiles) {
        if (!LittleFS.exists(filename)) continue;
        File file = LittleFS.open(filename, "r");
        if (!file) continue;

        DynamicJsonDocument doc(1024);
        if (deserializeJson(doc, file)) {
            file.close();
            continue;
        }
        file.close();

        String type = doc["sensor_type"].as<String>();
        int pin = doc["pin"] | -1;

        if (type == "pressure") {
            sensor_pressure = new PressureSensor(pin);
            sensor_pressure->configure(doc.as<JsonVariant>());
            _sensors.push_back(sensor_pressure);
        } 
        else if (type == "tds_sensor") {
            sensor_tds = new TdsSensor(pin);
            sensor_tds->configure(doc.as<JsonVariant>());
            _sensors.push_back(sensor_tds);
        }
        else if (type == "flow") {
            sensor_flow = new FlowSensor(pin);
            sensor_flow->configure(doc.as<JsonVariant>());
            _sensors.push_back(sensor_flow);
        } 
        else if (type == "temperature") {
            sensor_temperature = new TemperatureSensor(pin);
            sensor_temperature->configure(doc.as<JsonVariant>());
            _sensors.push_back(sensor_temperature);
        }
        // NOTA: O sensor de volume é ignorado nesta fase
    }

    // --- FASE 2: CRIAR SENSORES DEPENDENTES ---
    Serial.println("\n--- Fase 2: Inicializando sensores dependentes ---");
    for (const char* filename : configFiles) {
        if (!LittleFS.exists(filename)) continue;
        File file = LittleFS.open(filename, "r");
        if (!file) continue;

        DynamicJsonDocument doc(1024);
        if (deserializeJson(doc, file)) {
            file.close();
            continue;
        }
        file.close();

        String type = doc["sensor_type"].as<String>();

        if (type == "volume") {
            // Agora, quando chegamos aqui, o sensor_flow já foi garantidamente criado na Fase 1.
            if (sensor_flow == nullptr) {
                Serial.println("ERRO CRÍTICO: FlowSensor não foi encontrado para o VolumeSensor!");
                return false;
            }
            sensor_volume = new VolumeSensor(sensor_flow);
            sensor_volume->configure(doc.as<JsonVariant>());
            _sensors.push_back(sensor_volume);
        }
    }
    
    if (_sensors.empty()) {
        Serial.println("ERRO: Nenhum sensor foi carregado.");
        return false;
    }

    long minPeriodSec = 99999999;

    // 3. Itera sobre todos os objetos de sensor que foram criados
    for (const auto sensor : _sensors) {
        if (sensor) {
        // 4. Pede a cada sensor o seu período de amostragem
            long currentPeriod = sensor->getSamplingPeriod();
            
            // 5. Se o período deste sensor for menor que o mínimo que já encontrámos...
            if (currentPeriod < minPeriodSec) {
                // ... ele torna-se o novo mínimo.
                minPeriodSec = currentPeriod;
            }
        }
    }

    // 6. Converte para milissegundos e guarda na variável da classe
    if (minPeriodSec > 0) {
        _realtimeNotifyIntervalMs = minPeriodSec * 1000L;
    }

    _isReady = true; // Mesmo que algum sensor falhe, o hub fica pronto
    return true;
}

const HubBleConfig& DeviceController::getBleConfig() const { return _bleConfig; }
bool DeviceController::isReady() const { return _isReady; }
const std::vector<Sensor*>& DeviceController::getSensors() const { return _sensors; }
long DeviceController::getMinSamplingInterval(){
    return _realtimeNotifyIntervalMs;
}
