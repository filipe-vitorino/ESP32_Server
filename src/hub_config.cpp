#include "hub_config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

HubConfig& HubConfig::getInstance() {
    static HubConfig instance;
    return instance;
}

HubConfig::HubConfig() : _isLoaded(false) {}

bool HubConfig::load() {
    if (_isLoaded) return true;

    // Monta o LittleFS, formata se necessário
    if (!LittleFS.begin(true)) {
        Serial.println("Falha ao montar o LittleFS para HubConfig.");
        return false;
    }

    // Abre o arquivo de configuração
    if (!LittleFS.exists("/hub_config.json")) {
        Serial.println("ERRO: Arquivo /hub_config.json não encontrado.");
        return false;
    }

    File configFile = LittleFS.open("/hub_config.json", "r");
    if (!configFile) {
        Serial.println("ERRO: Não foi possível abrir /hub_config.json.");
        return false;
    }

    // Lê o arquivo inteiro para a string
    _jsonString = configFile.readString();
    configFile.close();

    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, _jsonString);

    if (error) {
        Serial.println("ERRO: Falha ao interpretar o /hub_config.json.");
        _jsonString = "{}"; // Limpa a string em caso de erro
        return false;
    }

    // Preenche a struct de detalhes
    _details.id = doc["hub_id"].as<String>();
    _details.name = doc["hub_name"].as<String>();
    _details.latitude = doc["location"]["latitude"] | 0.0;
    _details.longitude = doc["location"]["longitude"] | 0.0;

    //_rx_uuid = doc["ble"]["rx_characteristic_uuid"].as<String>();
   // _main_tx_uuid = doc["ble"]["main_tx_characteristic_uuid"].as<String>();
    _service_uuid = doc["ble"]["service_uuid"].as<String>();

    _isLoaded = true;
    Serial.println("HubConfig carregado com sucesso. ID do Hub: " + _details.id);
    return true;
}

const HubDetails& HubConfig::getDetails() const {
    return _details;
}

String HubConfig::getConfigJsonString() const {
    return _jsonString;
}

String HubConfig::getRxCharacteristicUuid() const { return _rx_uuid; }
String HubConfig::getMainTxCharacteristicUuid() const { return _main_tx_uuid; }
String HubConfig::getServiceUuid() const { return _service_uuid; }
