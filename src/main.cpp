#include <Arduino.h>
#include "ble_handler.h"
#include "wifi_handler.h"
#include "device_controller.h"
#include "hub_config.h" 
#include "data_logger.h"
#include "sensors/BaseSensor.h" 
#include "data_logger.h"
#include <LittleFS.h>
#include "rtc_service.h"
#include <Wire.h>

void generateTestLogs(DeviceController& device) {
  Serial.println("\n=========================================");
  Serial.println("INICIANDO GERAÇÃO DE LOGS DE TESTE...");
  Serial.println("=========================================");

  // Pega na lista de sensores que o DeviceController criou
  const auto& sensors = device.getSensors();
  if (sensors.empty()) {
    Serial.println("Nenhum sensor configurado. Teste abortado.");
    return;
  }

  // Define um timestamp inicial (ex: 1 de Outubro de 2025)
  time_t current_ts = 1760036598; 

  // Loop principal: 1 a 100
  for (int i = 0; i < 10; i++) {
    
    // Loop secundário: para cada sensor na nossa lista
    for (auto sensor : sensors) {
      if (sensor) {
        // Simula uma leitura de sensor
        int raw = random(1000, 3000);
        float value = sensor->getValue(raw); // Usamos o método de calibração real

        // Chama a nossa função de log com o timestamp controlado
        logSensorReading(current_ts, sensor->getSensorId(),sensor->getUnit(), raw, value);

        // Incrementa o tempo para o próximo registo
        current_ts += 5; 
      }
    }
    Serial.printf("Ciclo de geração %d/100 completo.\n", i + 1);
    delay(10); // Pequena pausa para não bloquear o serial
  }

  Serial.println("=========================================");
  Serial.println("GERAÇÃO DE LOGS DE TESTE CONCLUÍDA.");
  Serial.println("Pode reiniciar o dispositivo ou iniciar a sincronização.");
  Serial.println("=========================================");
}


void listAllFiles(const char* basePath, int indent = 0) {
    File dir = LittleFS.open(basePath);
    if (!dir || !dir.isDirectory()) {
        Serial.print("Falha ao abrir diretório: ");
        Serial.println(basePath);
        return;
    }

    File file = dir.openNextFile();
    while(file) {
        String name = String(file.name()); // caminho absoluto
        String prefix = String(' ', indent * 2); // identação

        if(file.isDirectory()) {
            Serial.print(prefix);
            Serial.print("Diretório: ");
            Serial.println(name);

            // Recursão: adiciona "/" ao final para garantir que seja interpretado como diretório
            String subPath = name;
            if(!subPath.endsWith("/")) subPath = "/" + subPath;

            
            // Importante: adicionar LOG_DIR se o diretório não estiver no caminho
            if(!subPath.startsWith("/logs")) subPath = "/logs" + subPath;

            listAllFiles(subPath.c_str(), indent + 1);
        } else {
            Serial.print(prefix);
            Serial.print("Arquivo: ");
            Serial.println(name);
        }
        file = dir.openNextFile();
    }

    dir.close();
}

void printJsonlFile(const char* filePath) {
    File file = LittleFS.open(filePath, "r");
    if(!file) {
        Serial.print("Falha ao abrir o arquivo: ");
        Serial.println(filePath);
        return;
    }

    Serial.print("Conteúdo de ");
    Serial.println(filePath);

    while(file.available()) {
        String line = file.readStringUntil('\n'); // lê uma linha do JSONL
        line.trim();
        if(line.length() == 0) continue;

        Serial.println(line); // imprime a linha crua

        // --- Opcional: interpretar como JSON ---
        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, line);
        if(!err) {
            // Exemplo: ler campos específicos
            const char* ts = doc["ts"];
            int raw = doc["raw"];
            float value = doc["value"];
            const char* unit = doc["unit"];
            Serial.printf("TS: %s | Raw: %d | Value: %.2f %s\n", ts, raw, value, unit);
        } else {
            Serial.println("⚠ Erro ao interpretar JSON desta linha");
        }
    }

    file.close();
}

RTCService rtcService;
DeviceController meuDevice;
bool isSystemReady = false;
const long LOG_INTERVAL = 30000; // Salva um novo dado a cada 30 segundos
unsigned long previousLogMillis = 0;

void setup() {
    Serial.begin(115200);
    Wire.begin(21, 22);
    delay(2000);
    rtcService.begin();
    rtcService.adjustToCompileTime();
    Serial.println("\nIniciando Sensor Hub...");

    // 1. Carrega a configuração do próprio Hub
    HubConfig::getInstance().load();
    

    // 2. Tenta inicializar o controlador dos sensores
    if (meuDevice.init()) {
        isSystemReady = true;
        Serial.println("DeviceController inicializado com sucesso.");

        // 3. Inicializa os outros sistemas
        setupDataLogger();

        setupBLE(meuDevice);
        setupWiFi(meuDevice);
        //deleteLogFiles();
        //listAllFiles("/logs");
        Serial.println("\n======================================");
        Serial.println(" Setup concluído. Dispositivo em operação.");
        Serial.println("======================================");
    } else {
        isSystemReady = false;
        Serial.println("\n!!! ERRO FATAL: Falha na inicialização do DeviceController. !!!");
    }
   // generateTestLogs(meuDevice);
   //listAllFiles("/");

   // printJsonlFile("/logs/2025_10_09/sensor_01.jsonl"); 
  // O setup termina aqui. O loop() ficará vazio.
   //  Serial.println("Dispositivo em modo de espera após o teste.");

}
void loop() {
    if (meuDevice.isReady()) {
    // Obtém a lista de sensores do controlador
    const auto& sensors = meuDevice.getSensors();
    
    // Itera sobre cada sensor e chama o seu método update().
    // Esta é a forma dinâmica e escalável de fazer o que você já fazia com os 'if's.
    for (auto sensor : sensors) {
      if (sensor) {
        sensor->update();
      }
    }
  }
  loopBLE(meuDevice);
}