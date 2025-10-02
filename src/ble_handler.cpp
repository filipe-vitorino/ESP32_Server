#include "ble_handler.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <ArduinoJson.h>

#include "data_logger.h"
#include "config_handler.h"


// --- UUIDs do Serviço e Características BLE ---
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_TX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_RX "f48ebb2c-442a-4732-b0b3-009758a2f9b1"

// --- Variáveis Globais de Estado ---
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
volatile bool syncRequested = false; 
volatile time_t syncSinceTimestamp = 0;
volatile bool realTimeStreamActive = false; // NOVA flag de estado
unsigned long lastRealTimeSent = 0; // Controlo de tempo para o fluxo
bool ackReceived = false;
volatile bool syncCancelled = false;
volatile bool configRequested = false; 
// --- Protótipo da função de sync ---
void handleSyncProcess();

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
          case 0x01: // ACK
            ackReceived = true;
            break;
          case 0x02: // Sincronizar Histórico Completo
            syncRequested = true;
            Serial.println("📲 ESP32: Comando de sync total (0x02) recebido!");
            break;
          case 0x03: // Iniciar Fluxo Tempo Real
            realTimeStreamActive = true;
            Serial.println("📲 Comando para INICIAR fluxo em tempo real recebido.");
            break;
          case 0x05: // Parar Fluxo Tempo Real
            realTimeStreamActive = false;
            Serial.println("📲 Comando para PARAR fluxo em tempo real recebido.");
            break;
          case 0x06: // NOVO: Comando para Apagar Log
            Serial.println("Comando para Deletar recebido.");
            deleteLogFile();
            break;
          case 0x07: // Comando para Cancelar Sincronização
            syncCancelled = true;
            Serial.println("📲 ESP32: Comando para CANCELAR sync (0x07) recebido!");
            break;
          case 0x20: // NOVO: Comando para Pedir Configuração
            configRequested = true;
            Serial.println("📲 ESP32: Comando para pedir config (0x20) recebido!");
            break;
        }
      }
    }
};


// --- Funções setupBLE, waitForAck, handleSyncProcess ---
// Nenhuma alteração nestas funções. Cole as suas versões completas aqui.
void setupBLE() {
  BLEDevice::init("ESP32_BLE_Sensor_Hub");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);

  pTxCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_RX,
                       BLECharacteristic::PROPERTY_WRITE
                     );
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  Serial.println("Servidor BLE iniciado. Aguardando conexão...");
}

bool waitForAck() {
  long startTime = millis();
  ackReceived = false;
  while(!ackReceived) {
    if (!deviceConnected) return false;
    if (millis() - startTime > 2000) { // Timeout de 2 segundos
      Serial.println("Timeout esperando por ACK.");
      return false;
    }
    delay(10);
  }
  return true;
}

void handleSyncProcess() {
  Serial.println("\n--- ESP32: Iniciando processo de sincronização total ---");
  syncCancelled = false;

  int totalRecords = getTotalRecords();
  Serial.printf("ESP32: Encontrados %d registos para enviar.\n", totalRecords);
  
  // Envia SOT
  StaticJsonDocument<100> sotDoc;
  sotDoc["type"] = "SOT";
  sotDoc["records"] = totalRecords;
  String sotStr;
  serializeJson(sotDoc, sotStr);
  pTxCharacteristic->setValue(sotStr.c_str());
  pTxCharacteristic->notify();

  if (totalRecords > 0 && waitForAck()) {
    openLogFileForRead();
    for (int i=0; i < totalRecords; i++) {
      if (syncCancelled) {
        Serial.println("⚠️ Sincronização cancelada pelo utilizador. A abortar loop.");
        break; // Sai do loop de envio
      }
      if (!deviceConnected) { Serial.println("Conexão perdida, abortando."); break; }
      String entry = readNextLogEntry();
      if (entry.length() > 2) {
          String dataStr = "{\"type\":\"data\"," + entry.substring(1);
          pTxCharacteristic->setValue(dataStr.c_str());
          pTxCharacteristic->notify();
          if (!waitForAck()) { Serial.println("Falha no ACK. Abortando."); break; }
      }
    }
    closeLogFile();
  } else if (totalRecords > 0) {
      Serial.println("Falha no ACK para o SOT. Abortando.");
  }

  // Envia EOT para finalizar
  StaticJsonDocument<100> eotDoc;
  eotDoc["type"] = "EOT";
  String eotStr;
  serializeJson(eotDoc, eotStr);
  Serial.printf("📤 ESP32: A enviar pacote EOT: %s\n", eotStr.c_str());
  pTxCharacteristic->setValue(eotStr.c_str());
  pTxCharacteristic->notify();
  
  Serial.println("--- ESP32: Processo de sincronização finalizado. ---");/* ... (código do EOT sem alterações) ... */
  Serial.println("--- ESP32: Sincronização finalizada. Aguardando comando de apagar. ---");
}

// --- loopBLE com a Nova Máquina de Estados ---
void loopBLE() {
  if (!deviceConnected) return;

  // Prioridade 1: Verificar se uma sincronização foi pedida
  if (syncRequested) {
    realTimeStreamActive = false; // Garante que o fluxo em tempo real pare durante o sync
    time_t ts = syncSinceTimestamp;
    syncRequested = false;
    handleSyncProcess();
  } 
  // Prioridade 2: Se não houver sync, verifica se o fluxo em tempo real está ativo
  else if (realTimeStreamActive) {
    // Envia dados a cada 2 segundos
    if (millis() - lastRealTimeSent > 2000) {
      lastRealTimeSent = millis();

      StaticJsonDocument<200> doc;
      doc["ts"] = time(nullptr);
      doc["vazao"] = random(100, 200) / 10.0;
      doc["temperatura"] = random(200, 300) / 10.0;
      doc["volume"] = random(300, 400) / 10.0;
      doc["pressao"] = random(10, 40) / 10.0;
      doc["tds"] = random(200, 600)/10.0;
      // Adicione os outros campos se desejar
      
      String output;
      serializeJson(doc, output);
      pTxCharacteristic->setValue(output.c_str());
      pTxCharacteristic->notify();
      Serial.printf("📤 Enviando dado em tempo real: %s\n", output.c_str());
    }
  } else if(configRequested){
       configRequested = false; // Limpa a flag
      
      // ==========================================================
      //      >>> LÓGICA CORRIGIDA AQUI <<<
      // ==========================================================
      
      // 1. Obtém a string JSON do ficheiro de configuração
      String configString = getConfigJsonString();
      
      // 2. Cria um JsonDocument temporário para analisar a configuração
      StaticJsonDocument<512> configDoc; // O tamanho deve ser suficiente para o config.json
      DeserializationError error = deserializeJson(configDoc, configString);

      if (error) {
        Serial.print(F("deserializeJson() falhou: "));
        Serial.println(error.c_str());
      } else {
        // 3. Cria o JsonDocument principal (o pacote a ser enviado)
        StaticJsonDocument<768> mainDoc;
        mainDoc["type"] = "config";
        
        // 4. Atribui o documento de configuração ao campo "data" do documento principal
        mainDoc["data"] = configDoc;

        String output;
        serializeJson(mainDoc, output);
        
        pTxCharacteristic->setValue(output.c_str());
        pTxCharacteristic->notify();
        Serial.printf("📤 Enviando configuração via BLE: %s\n", output.c_str());
      }

  }

  // Se nenhuma flag estiver ativa, o ESP32 simplesmente espera por comandos.
  delay(10); // Pequena pausa para o processador respirar
}