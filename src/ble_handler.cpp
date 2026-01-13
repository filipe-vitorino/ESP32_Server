#include "ble_handler.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <ArduinoJson.h>
#include <BLE2902.h>
// Os arquivos de cabeçalho que nosso novo código precisa
#include "device_controller.h" 
#include "data_logger.h" // Assumindo que você tem este arquivo
#include "hub_config.h"

#include <map> // Incluímos a biblioteca para o mapa
#include <LittleFS.h>
// --- Acesso às Instâncias Globais ---


#define HUB_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define HUB_CHARACTERISTIC_RX   "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define HUB_CHARACTERISTIC_TX   "f48ebb2c-442a-4732-b0b3-009758a2f9b1"

extern DeviceController meuDevice;

extern bool isSystemReady;

BLECharacteristic *pTxCharacteristic;
std::map<String, BLECharacteristic*> characteristicMap;
bool deviceConnected = false;
volatile bool syncRequested = false; 
volatile time_t syncSinceTimestamp = 0;
volatile bool realTimeStreamActive = true;
unsigned long lastRealTimeSent = 0;
bool ackReceived = false;
volatile bool syncCancelled = false;
volatile bool configRequested = false; 

// --- Protótipo da função de sync ---
void handleSyncProcess();


// --- Callbacks (do seu arquivo original) ---
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; Serial.println("Dispositivo BLE conectado."); }
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false; syncRequested = false; realTimeStreamActive = false;
      Serial.println("Dispositivo BLE desconectado.");
      Serial.println("Recomeçando advertising...");
      BLEDevice::startAdvertising();
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      if (value.length() == 1) { // Apenas comandos de 1 byte
        switch(value[0]) {
          case 0x01: ackReceived = true; break;
          case 0x02: syncRequested = true; Serial.println("📲 Comando de sync total (0x02) recebido!"); break;
          case 0x03: 
            realTimeStreamActive = true;
            Serial.println("📲 Comando para INICIAR fluxo em tempo real recebido.");
            for (Sensor* s : meuDevice.getSensors()) {
              if (s) s->notify();
            }
            break;

            case 0x05: realTimeStreamActive = false; Serial.println("📲 Comando para PARAR fluxo em tempo real recebido."); break;
          case 0x06: Serial.println("Comando para Deletar recebido."); deleteLogFiles(); break;
          case 0x07: syncCancelled = true; Serial.println("📲 Comando para CANCELAR sync (0x07) recebido!"); break;
          case 0x20: configRequested = true; Serial.println("📲 Comando para pedir config (0x20) recebido!"); break;
        }
      }
    }
};

// --- Função auxiliar para getConfigJsonString (para compatibilidade) ---
String getConfigJsonString() {
    return "";// Adaptação simples
}

void printCharacteristicInfo(BLECharacteristic* pChar) {
    if (!pChar) {
        Serial.println("[BLE] ❌ Característica nula!");
        return;
    }

    // UUID da característica
    Serial.printf("  🔹 UUID: %s\n", pChar->getUUID().toString().c_str());

}

void setupBLE(DeviceController& meuDevice) {
    if (!meuDevice.isReady()) {
        Serial.println("BLE Setup abortado: DeviceController não está pronto.");
        return;
    }


    // UUID para o serviço de sensores
    const String sensorServiceUuid = HubConfig::getInstance().getServiceUuid();


    // Inicializa o BLE
    BLEDevice::init("ESP32_BLE_01");
    BLEServer* pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

     // ===========================
    // Serviço HUB (TX/RX)
    // ===========================
    BLEService* pHubService = pServer->createService(HUB_SERVICE_UUID);

    BLECharacteristic* pRxCharacteristic = pHubService->createCharacteristic(
        HUB_CHARACTERISTIC_RX,
        BLECharacteristic::PROPERTY_WRITE
    );
    pRxCharacteristic->setCallbacks(new MyCallbacks());

    pTxCharacteristic = pHubService->createCharacteristic(
        HUB_CHARACTERISTIC_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTxCharacteristic->addDescriptor(new BLE2902());

    pHubService->start();

    // ===========================
    // Serviço Sensores
    // ===========================
    BLEService* pSensorService = pServer->createService(
        BLEUUID(sensorServiceUuid.c_str()), 
        30  // aumenta o limite de handlers
    );

    Serial.println("Criando características de dados dos sensores dinamicamente...");
    const auto& sensors = meuDevice.getSensors();
    for (Sensor* s : sensors) {
        if (!s) continue;

        String sensorId = s->getSensorId();
        String charUuid = s->getCharacteristicUuid();

        BLECharacteristic* pChar = pSensorService->createCharacteristic(
            charUuid.c_str(),
            BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
        );
        pChar->addDescriptor(new BLE2902());
        delay(10);
        if (pChar->getDescriptorByUUID(BLEUUID((uint16_t)0x2902)) == nullptr) {
            Serial.printf("⚠️  Falha ao adicionar BLE2902 em %s\n", sensorId.c_str());
        } else {
            Serial.printf("✅ BLE2902 adicionado em %s\n", sensorId.c_str());
        }

        // Salva no mapa para notificação futura
        characteristicMap[sensorId] = pChar;

        Serial.printf("🔹 Característica criada para sensor %s (UUID: %s)\n",
                      sensorId.c_str(), charUuid.c_str());
    }

    Serial.println("🧩 Resumo das características criadas:");
    for (auto const& entry : characteristicMap) {
        Serial.printf("   ↳ SensorID: %s | Char UUID: %s\n",
                      entry.first.c_str(),
                      entry.second->getUUID().toString().c_str());
    }

    pSensorService->start();

    // ===========================
    // Advertising
    // ===========================
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(HUB_SERVICE_UUID);
    pAdvertising->addServiceUUID(sensorServiceUuid.c_str());
    BLEDevice::startAdvertising();

    Serial.println("Servidor BLE iniciado com HUB + Sensores.");
}
bool waitForAck() {
  long startTime = millis();
  ackReceived = false;
  while(!ackReceived) {
    if (!deviceConnected) return false;
    if (millis() - startTime > 2000) {
      Serial.println("Timeout esperando por ACK.");
      return false;
    }
    delay(10);
  }
  return true;
}

void handleSyncProcess() {
    Serial.println("\n--- ESP32: Iniciando sync de MÚLTIPLOS FICHEIROS via BLE ---");

    int totalRecords = getTotalRecordsInAllFiles();
    Serial.printf("ℹ️ ESP32: Encontrados %d registros no total para enviar.\n", totalRecords);

    // 1. Envia SOT
    StaticJsonDocument<100> sotDoc;
    sotDoc["type"] = "SOT";
    sotDoc["records"] = totalRecords;
    String sotStr;
    serializeJson(sotDoc, sotStr);

    // ** Adiciona quebra de linha ao JSON antes de enviar **
    if (!sotStr.endsWith("\n")) sotStr += '\n';

    pTxCharacteristic->setValue(sotStr.c_str());
    pTxCharacteristic->notify();

    if (!waitForAck()) {
        Serial.println("❌ Falha no ACK para o SOT ou nenhum registro encontrado. Abortando.");
        return;
    }
    Serial.println("✅ ACK para SOT recebido. Iniciando envio de dados...");

    int totalCount = 0;
    File root = LittleFS.open(LOG_DIR);
    if (!root || !root.isDirectory()) {
        Serial.println("❌ Falha ao abrir LOG_DIR ou não é diretório.");
        return ;
    }

    File dir = root.openNextFile();
    while(dir) {
        if(dir.isDirectory()) {
            Serial.printf("📁 Diretório: %s\n", dir.name());

            File f = dir.openNextFile();
            while(f) {
                if(!f.isDirectory()) {
                    Serial.printf("   📄 Arquivo: %s\n", f.name());

                    while(f.available()) {
                        String line = f.readStringUntil('\n');
                        Serial.printf("      📝 Linha lida: %s\n", line.c_str());
                        if(line.length() > 2){
                          String jsonStr = "{\"type\":\"data\"," + line.substring(1);

                          // ** Adiciona quebra de linha ao JSON antes de enviar **
                          if (!jsonStr.endsWith("\n")) jsonStr += '\n';

                          Serial.printf("JSOON: %s\n", jsonStr.c_str());
                            pTxCharacteristic->setValue(jsonStr.c_str());
                            pTxCharacteristic->notify();

                            if (!waitForAck()) {
                                Serial.println("❌ Timeout ou falha no ACK. Interrompendo envio.");
                                f.close();
                                dir.close();
                                root.close();
                                return;
                            }
                        }
                    }
                }
                f.close();
                f = dir.openNextFile();
            }
        }
        dir.close();
        dir = root.openNextFile();
    }
    root.close();
    Serial.printf("ℹ️ Total de registros encontrados: %d\n", totalCount);

    // 3. Envia EOT
    StaticJsonDocument<100> eotDoc;
    eotDoc["type"] = "EOT";
    String eotStr;
    serializeJson(eotDoc, eotStr);

    // ** Adiciona quebra de linha ao JSON antes de enviar **
    if (!eotStr.endsWith("\n")) eotStr += '\n';

    pTxCharacteristic->setValue(eotStr.c_str());
    pTxCharacteristic->notify();

    Serial.println("--- ESP32: Sincronização de múltiplos ficheiros finalizada. ---");
}
void notifySensorValue(const String& sensor_id, float value, const String& unit) {
   // if (!deviceConnected) return;
    //value = random(100,300)/10;
    // Cria JSON
    StaticJsonDocument<128> doc;
    doc["sensorId"] = sensor_id;
    doc["value"] = value;
    doc["unit"] = unit;

    String jsonStr;
    serializeJson(doc, jsonStr);

    // ** Adiciona quebra de linha ao JSON antes de enviar **
    if (!jsonStr.endsWith("\n")) jsonStr += '\n';

    // Procura a característica correta
    auto it = characteristicMap.find(sensor_id);
    if (it != characteristicMap.end()) {
        BLECharacteristic* pCharacteristic = it->second;
        pCharacteristic->setValue(jsonStr.c_str());
        pCharacteristic->notify();

        Serial.printf("📤 Notificando sensor %s: %s\n", sensor_id.c_str(), jsonStr.c_str());
    }
}

void sendJsonInChunks(BLECharacteristic* pChar, const String& json) {
    if (!pChar) return;

    // ** Garante quebra de linha no final também para o envio fragmentado **
    String jsonToSend = json;
    if (!jsonToSend.endsWith("\n")) jsonToSend += '\n';

    const int chunkSize = 500; // Tamanho do fragmento (ajustar conforme teste)
    int len = jsonToSend.length();

    Serial.printf("📤 Enviando JSON em %d bytes, fragmentando em %d bytes...\n", len, chunkSize);

    for (int i = 0; i < len; i += chunkSize) {
        String part = jsonToSend.substring(i, min(i + chunkSize, len));
        pChar->setValue(part.c_str());
        pChar->notify();
        delay(10); // Pequeno delay para o BLE não travar
    }

    Serial.println("✅ JSON enviado em fragments com sucesso.");
}

// --- loopBLE com a Nova Máquina de Estados ---
void loopBLE(DeviceController& meuDevice) {
  if (!deviceConnected) return;

  if (syncRequested) {
    realTimeStreamActive = false;
    syncRequested = false;
    configRequested = false;
    handleSyncProcess();
  } else if(configRequested){
    realTimeStreamActive = false;
    syncRequested = false;
    configRequested = false;
        // Pega a string JSON da configuração do Hub (ex: {"hub_id": "...", ...})
    String configString = HubConfig::getInstance().getConfigJsonString();

    // --- CORREÇÃO DA LÓGICA DE ANINHAMENTO ---

    // 1. Crie um documento temporário para analisar a 'configString'
    StaticJsonDocument<512> dataDoc; // Escolha um tamanho adequado para a config do hub
    DeserializationError error = deserializeJson(dataDoc, configString);

    if (error) {
        Serial.print("Falha ao analisar a string de configuração interna: ");
        Serial.println(error.c_str());
    } else {
        // 2. Crie o documento principal que será enviado
        StaticJsonDocument<2048> mainDoc; // Precisa ser grande o suficiente para conter o outro doc
        mainDoc["type"] = "config";
        
        // 3. Atribua o documento já analisado (dataDoc) ao campo "data". 
        // A biblioteca ArduinoJson cuidará de fazer a cópia dos dados.
        mainDoc["data"] = dataDoc;

        Serial.println("📋 Sensores antes de montar JSON:");
        for (auto& sensor : meuDevice.getSensors()) {
            Serial.printf("  ID: %s \n", sensor->getSensorId().c_str());
        }

        if (!meuDevice.getSensors().empty()) {
            JsonArray sensorsArray = mainDoc.createNestedArray("sensors");
            for (Sensor* s : meuDevice.getSensors()) {
                if (!s) continue;
                s->toConfigJson(sensorsArray); // adiciona diretamente no array
            }
        }


        String output;
        serializeJson(mainDoc, output);

        // ** Se for enviar via setValue, garantir nova linha; aqui usamos sendJsonInChunks (que já garante '\n') **
        if(pTxCharacteristic) {
            sendJsonInChunks(pTxCharacteristic, output);
            Serial.printf("📤 Enviando configuração aninhada via BLE: %s\n", output.c_str());
        }

    }
  }
  

  delay(10);
}
