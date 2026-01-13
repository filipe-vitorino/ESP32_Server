#ifndef HUB_CONFIG_H
#define HUB_CONFIG_H

#include <Arduino.h>

// Struct para organizar os dados lidos
struct HubDetails {
    String id;
    String name;
    float latitude;
    float longitude;
};

class HubConfig {
public:
    // Padrão Singleton para garantir uma única instância
    static HubConfig& getInstance();

    // Carrega o arquivo /hub_config.json do SPIFFS
    bool load();

    // Getters para acessar os dados
    const HubDetails& getDetails() const;
    
    // A função que o ble_handler precisa para enviar a configuração
    String getConfigJsonString() const;

    String getRxCharacteristicUuid() const;
    String getMainTxCharacteristicUuid() const;
    String getServiceUuid() const;

private:
    HubConfig(); // Construtor privado
    HubConfig(const HubConfig&) = delete;
    void operator=(const HubConfig&) = delete;

    HubDetails _details;
    String _jsonString; // Armazena a string JSON para envio rápido
    bool _isLoaded;

    String _rx_uuid;
    String _main_tx_uuid;
    String _service_uuid;
};

#endif // HUB_CONFIG_H