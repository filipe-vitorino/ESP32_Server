# Documentação das Funções do Firmware ESP32

> Firmware C++ para Arduino/ESP32 — Hub de monitoramento de sensores com comunicação BLE e Wi-Fi

---

## Sumário

- [Arquitetura Geral](#arquitetura-geral)
- [Sensors (Hierarquia de Classes)](#sensors-hierarquia-de-classes)
  - [Sensor (BaseSensor)](#sensor-basesensor)
  - [FlowSensor](#flowsensor)
  - [PressureSensor](#pressuresensor)
  - [TdsSensor](#tdssensor)
  - [TemperatureSensor](#temperaturesensor)
  - [VolumeSensor](#volumesensor)
- [DeviceController](#devicecontroller)
- [HubConfig](#hubconfig)
- [RTCService](#rtcservice)
- [DataLogger](#datalogger)
- [BleHandler](#blehandler)
- [WifiHandler](#wifihandler)
- [main.cpp](#maincpp)

---

## Arquitetura Geral

```
main.cpp
 ├── setup()  → HubConfig::load() → DeviceController::init() → setupDataLogger() → setupBLE() → setupWiFi()
 └── loop()   → sensor->update() [para cada sensor] → loopBLE()

DeviceController
 └── std::vector<Sensor*> _sensors
      ├── FlowSensor
      ├── VolumeSensor  (depende de FlowSensor)
      ├── TemperatureSensor
      ├── PressureSensor
      └── TdsSensor

DataLogger  ←  Sensor::update() chama logSensorReading()
BleHandler  ←  Sensor::update() chama notifySensorValue()
WifiHandler ←  Serve endpoints HTTP para o app móvel
```

---

## Sensors (Hierarquia de Classes)

### Sensor (BaseSensor)

Classe base abstrata da qual todos os sensores herdam. Define o contrato de ciclo de vida e os dados comuns.

#### Métodos Públicos

| Método | Assinatura | Descrição |
|---|---|---|
| `configure` | `void configure(const JsonVariant& configJson)` | Lê o arquivo JSON do sensor e preenche os campos comuns: `_sensor_type`, `_sensor_id`, `_pin`, `_unit`, `_sampling_period_sec`, `_ble_characteristic_uuid`, `_valorCriticoMin/Max`. Inicializa o temporizador para permitir a primeira leitura imediata. Ao final, chama `_configureCalibration()` virtual para que a subclasse configure seus parâmetros específicos. |
| `update` | `virtual void update()` | Método principal chamado pelo `loop()`. Obtém o timestamp atual do RTC, lê o valor bruto (`getRaw()`) e o valor calibrado (`getValue()`). Salva a leitura no log se o período de amostragem tiver passado (`logSensorReading()`). Envia notificação BLE a cada 2 segundos (`notifySensorValue()`). |
| `getRaw` | `virtual int getRaw() = 0` | **Puro virtual.** Cada subclasse implementa a leitura bruta do hardware (pino analógico, pulsos, etc.). |
| `getValue` | `virtual float getValue(int rawValue) = 0` | **Puro virtual.** Cada subclasse aplica a fórmula de calibração ao valor bruto e retorna o valor físico final. |
| `notify` | `void notify()` | Força uma notificação BLE imediata: lê o valor bruto e calibrado e chama `notifySensorValue()`. Usado na inicialização do streaming em tempo real (comando `0x03`). |
| `readNow` | `void readNow()` | Faz uma leitura fresca e imediata sem enviar notificação nem salvar log. Armazena o resultado em `_lastValue`. Usado pelo endpoint `/dados` do Wi-Fi. |
| `toConfigJson` | `virtual void toConfigJson(JsonArray& array)` | Serializa a configuração do sensor como um objeto no array JSON fornecido. Campos: `sensor_id`, `sensorType`, `unit`, `uuid_c`, `valor_critico` (min/max). Chamado na resposta ao comando de configuração BLE e no endpoint `/config` Wi-Fi. |
| `getSensorId` | `String getSensorId() const` | Retorna o identificador único do sensor (`_sensor_id`). |
| `getCharacteristicUuid` | `String getCharacteristicUuid() const` | Retorna o UUID da característica BLE associada ao sensor. |
| `getUnit` | `String getUnit() const` | Retorna a unidade de medida do sensor (ex: `°C`, `L/min`). |
| `getSensorType` | `String getSensorType() const` | Retorna o tipo do sensor (ex: `temperatura`, `vazao`). |
| `getSamplingPeriod` | `long getSamplingPeriod() const` | Retorna o período de amostragem em segundos. Usado pelo `DeviceController` para calcular o menor intervalo entre todos os sensores. |
| `getLastValue` | `float getLastValue() const` | Retorna o último valor calibrado calculado, sem fazer nova leitura de hardware. |

#### Métodos Protegidos

| Método | Assinatura | Descrição |
|---|---|---|
| `_configureCalibration` | `virtual void _configureCalibration(const JsonVariant& calibrationConfig) = 0` | **Puro virtual.** Chamado por `configure()`. Cada subclasse lê os parâmetros de calibração específicos do campo `calibration` do JSON. |

---

### FlowSensor

Mede a vazão de fluido por contagem de pulsos em uma interrupção de hardware (ISR).

| Método | Assinatura | Descrição |
|---|---|---|
| `FlowSensor` (construtor) | `FlowSensor(uint8_t pin)` | Configura o pino como `INPUT_PULLUP`, define o ponteiro global `_instanceForISR` para si mesmo e registra `_pulseISR` como rotina de interrupção na borda de subida (`RISING`). |
| `_pulseISR` | `static void _pulseISR()` | Rotina de serviço de interrupção (ISR) estática. Incrementa `_pulseCount` via ponteiro global a cada pulso detectado no pino. Deve ser estática pois ISRs não podem ter contexto de objeto. |
| `_configureCalibration` | `void _configureCalibration(const JsonVariant& calibrationConfig)` | Lê `factor` (fator de conversão pulsos→L/min), `valid_range.min` e `valid_range.max` do JSON de calibração. |
| `getRaw` | `int getRaw()` | Desabilita interrupções (`noInterrupts()`), copia e zera `_pulseCount`, reabilita interrupções (`interrupts()`), e retorna o número de pulsos acumulados desde a última leitura. |
| `getValue` | `float getValue(int rawValue)` | Multiplica os pulsos pelo `_factor` para obter o fluxo em L/min. Aplica `constrain` ao resultado dentro de `[_rangeMin, _rangeMax]`. |

---

### PressureSensor

Mede pressão via leitura analógica com mapeamento linear de faixa calibrada.

| Método | Assinatura | Descrição |
|---|---|---|
| `PressureSensor` (construtor) | `PressureSensor(uint8_t pin)` | Armazena o pino e inicializa os valores padrão de segurança: `_zero=0`, `_cem=4095` (máximo ADC 12-bit do ESP32), `_rangeMin=0`, `_rangeMax=100`. |
| `_configureCalibration` | `void _configureCalibration(const JsonVariant& calibrationConfig)` | Lê `low_pressure_value` (→`_zero`), `high_pressure_value` (→`_cem`), `valid_range.min` e `valid_range.max` do JSON. |
| `getRaw` | `int getRaw()` | Executa `analogRead(_pin)` e retorna o valor ADC bruto (0–4095). |
| `getValue` | `float getValue(int rawValue)` | Aplica `constrain` ao valor bruto dentro de `[_zero, _cem]` e depois `map()` para converter para a faixa `[_rangeMin, _rangeMax]`. |

---

### TdsSensor

Mede Total de Sólidos Dissolvidos (TDS) via leitura analógica com calibração linear ou polinomial configurável.

| Método | Assinatura | Descrição |
|---|---|---|
| `TdsSensor` (construtor) | `TdsSensor(uint8_t pin)` | Armazena o pino e inicializa `_rangeMin=-50.0` e `_rangeMax=125.0` como valores padrão. |
| `_configureCalibration` | `void _configureCalibration(const JsonVariant& calibrationConfig)` | Lê o campo `type` para definir o modo (`"linear"` ou `"polynomial"`). Para polinomial: extrai o array `coefficients` e o `factor`. Para linear: extrai os coeficientes `a` e `b` de um objeto. Lê `valid_range.min/max` para os limites de saída. |
| `getRaw` | `int getRaw()` | Executa `analogRead(_pin)` e retorna o valor ADC bruto. |
| `getValue` | `float getValue(int rawValue)` | Aplica a fórmula de calibração selecionada: linear (`a*x + b`) ou polinomial (avaliação de Horner sobre os coeficientes). Aplica `constrain` ao resultado dentro de `[_rangeMin, _rangeMax]`. |

---

### TemperatureSensor

Mede temperatura via sensor digital DS18B20 no protocolo 1-Wire (biblioteca DallasTemperature).

| Método | Assinatura | Descrição |
|---|---|---|
| `TemperatureSensor` (construtor) | `TemperatureSensor(uint8_t pin)` | Armazena o pino e inicializa os ponteiros `_oneWire` e `_sensors` como `nullptr` e os limites padrão `[-50, 125]` °C. |
| `_configureCalibration` | `void _configureCalibration(const JsonVariant& calibrationConfig)` | Lê `unit`, `index` (índice do sensor no barramento 1-Wire) e `valid_range.min/max`. Instancia `OneWire` e `DallasTemperature` com o pino configurado e chama `_sensors->begin()`. |
| `getRaw` | `int getRaw()` | Retorna sempre `0`, pois o DS18B20 não possui valor ADC bruto. Mantido apenas para satisfazer a assinatura virtual da classe base. |
| `getValue` | `float getValue(int rawValue)` | Chama `requestTemperatures()` no barramento e lê a temperatura em Celsius pelo índice `_index`. Aplica `constrain` ao resultado dentro de `[_rangeMin, _rangeMax]`. Retorna `0.0` se o sensor não estiver inicializado. |

---

### VolumeSensor

Calcula o volume acumulado de fluido ao longo do tempo, derivado das leituras do `FlowSensor`. Sobrescreve o `update()` da classe base.

| Método | Assinatura | Descrição |
|---|---|---|
| `VolumeSensor` (construtor) | `VolumeSensor(FlowSensor* flowSensor)` | Recebe o ponteiro para o `FlowSensor` do qual depende e inicializa `_accumulatedVolume=0`, `_rangeMin=0`, `_rangeMax=1000`. |
| `_configureCalibration` | `void _configureCalibration(const JsonVariant& calibrationConfig)` | Lê `valid_range.min/max` para os limites do volume acumulado. |
| `getRaw` | `int getRaw()` | Retorna o volume acumulado em mililitros (conversão de `_accumulatedVolume * 1000`) como inteiro. |
| `getValue` | `float getValue(int rawValue)` | Retorna `_accumulatedVolume` em litros diretamente, sem processamento adicional. |
| `update` | `void update()` (override) | Sobrescreve o `update()` da classe base. A cada período de amostragem: lê os pulsos via `_flowSensor->getRaw()`, converte para L/min com `_flowSensor->getValue()`, calcula o volume do intervalo (`fluxo * tempo_em_min`) e o acumula em `_accumulatedVolume`. Aplica constrain. Chama `notifySensorValue()` e `logSensorReading()` diretamente. |

---

## DeviceController

Gerencia o ciclo de vida de todos os sensores. Carrega os arquivos de configuração JSON do LittleFS em duas fases para resolver dependências entre sensores.

| Método | Assinatura | Descrição |
|---|---|---|
| `DeviceController` (construtor) | `DeviceController()` | Inicializa todos os ponteiros de sensor como `nullptr` e `_isReady` como `false`. |
| `~DeviceController` (destrutor) | `~DeviceController()` | Itera sobre `_sensors`, deleta cada objeto e limpa o vetor. Garante que não haja vazamento de memória. |
| `init` | `bool init()` | **Fase 1:** monta o LittleFS e itera sobre os arquivos de config (`/vazao.json`, `/volume.json`, `/temperatura.json`, `/pressao.json`, `/tds.json`). Para cada arquivo existente, identifica o `sensor_type`, instancia a subclasse correta (`new PressureSensor`, `new TdsSensor`, etc.), chama `configure()` e adiciona ao vetor `_sensors`. **Fase 2:** cria o `VolumeSensor` passando o `FlowSensor` já criado (retorna `false` se o FlowSensor não existir). Após ambas as fases, calcula o menor `getSamplingPeriod()` entre todos os sensores para `_realtimeNotifyIntervalMs`. Define `_isReady = true`. |
| `getBleConfig` | `const HubBleConfig& getBleConfig() const` | Retorna a struct `HubBleConfig` com os UUIDs BLE do Hub. |
| `isReady` | `bool isReady() const` | Retorna `true` se `init()` concluiu com sucesso. Usado como guarda em `setupBLE()` e no `loop()`. |
| `getSensors` | `const std::vector<Sensor*>& getSensors() const` | Retorna referência constante ao vetor de ponteiros de sensor. Usado pelo `loop()`, `setupBLE()` e pelos endpoints Wi-Fi. |
| `getMinSamplingInterval` | `long getMinSamplingInterval()` | Retorna o menor período de amostragem entre todos os sensores em milissegundos. Exposto pelo endpoint `/config` para o app calibrar o polling. |

---

## HubConfig

Singleton que carrega e expõe a configuração estática do hub a partir do arquivo `/hub_config.json` no LittleFS.

| Método | Assinatura | Descrição |
|---|---|---|
| `getInstance` | `static HubConfig& getInstance()` | Retorna a instância única (padrão Meyers Singleton). Thread-safe em C++11+. |
| `load` | `bool load()` | Monta o LittleFS, lê `/hub_config.json`, desserializa com ArduinoJson e preenche `_details` (id, name, latitude, longitude) e os UUIDs BLE (`_service_uuid`). Armazena a string JSON bruta em `_jsonString` para envio rápido. Retorna `false` se o arquivo não existir, não puder ser aberto ou o JSON for inválido. Idempotente: retorna `true` imediatamente se já foi carregado. |
| `getDetails` | `const HubDetails& getDetails() const` | Retorna a struct `HubDetails` com id, name, latitude e longitude do hub. |
| `getConfigJsonString` | `String getConfigJsonString() const` | Retorna a string JSON bruta do arquivo de configuração. Usado pelo `BleHandler` para construir a resposta ao comando de configuração (`0x20`). |
| `getRxCharacteristicUuid` | `String getRxCharacteristicUuid() const` | Retorna o UUID da característica RX BLE. |
| `getMainTxCharacteristicUuid` | `String getMainTxCharacteristicUuid() const` | Retorna o UUID da característica TX principal BLE. |
| `getServiceUuid` | `String getServiceUuid() const` | Retorna o UUID do serviço BLE de sensores. Usado em `setupBLE()` para criar o serviço dinâmico de características de sensor. |

---

## RTCService

Encapsula o módulo de relógio em tempo real DS3231 via I²C (biblioteca RTClib).

| Método | Assinatura | Descrição |
|---|---|---|
| `RTCService` (construtor) | `RTCService()` | Inicializa `initialized = false`. |
| `begin` | `bool begin()` | Inicia o barramento I²C com `Wire.begin()` e tenta inicializar o RTC DS3231. Define `initialized = true` em sucesso. Retorna `false` se o módulo não responder. |
| `adjustToCompileTime` | `void adjustToCompileTime()` | Ajusta o RTC para a data e hora em que o firmware foi compilado (`__DATE__` e `__TIME__`). Útil para configuração inicial. Não executa se o RTC não estiver inicializado. |
| `setDateTime` | `void setDateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)` | Ajusta o RTC para a data e hora fornecidas manualmente. Não executa se o RTC não estiver inicializado. |
| `getRealTime` | `String getRealTime()` | Retorna a data e hora atuais formatadas como `"DD/MM/YYYY HH:MM:SS"`. Retorna `"RTC_NOT_INITIALIZED"` se o módulo não estiver pronto. |
| `getTimestamp` | `time_t getTimestamp()` | Retorna o Unix timestamp (segundos desde 1970-01-01) atual do RTC. Retorna `0` se não inicializado. Chamado por `Sensor::update()` e `VolumeSensor::update()` para carimbar cada leitura salva no log. |

---

## DataLogger

Gerencia a persistência das leituras de sensores no sistema de arquivos LittleFS, organizadas em subpastas diárias com arquivos JSONL por sensor.

**Estrutura de arquivos:**
```
/logs/
  2025_10_09/
    sensor_flow.jsonl
    sensor_temp.jsonl
  2025_10_10/
    sensor_flow.jsonl
    ...
```

| Função | Assinatura | Descrição |
|---|---|---|
| `setupDataLogger` | `void setupDataLogger()` | Monta o LittleFS (formatando se necessário) e cria o diretório raiz `/logs` se não existir. |
| `logSensorReading` | `void logSensorReading(time_t timestamp, const String& sensorId, const String& sensorType, const String& unit, int rawValue, float calibratedValue)` | Valida que o timestamp é posterior a 2024-01-01 (rejeita hora inválida). Cria o subdiretório diário (`/logs/YYYY_MM_DD/`) se necessário. Abre ou cria o arquivo `/<sensorId>.jsonl` em modo append. Serializa o registro como JSON com os campos `ts` (ISO 8601), `sensorId`, `sensorType`, `raw`, `value` e `unit`, e grava uma linha por registro. |
| `setSystemTime` | `void setSystemTime(time_t epochTime)` | Acerta o relógio do sistema do ESP32 usando `settimeofday()` com o Unix timestamp fornecido. Imprime no Serial a hora ajustada. |
| `deleteLogFiles` | `void deleteLogFiles()` | Percorre recursivamente `/logs`, apaga todos os arquivos `.jsonl` e remove os diretórios diários vazios. Chamado pelo comando BLE `0x06` e pelo endpoint Wi-Fi `/limpar_historico`. |
| `getTotalRecordsInAllFiles` | `int getTotalRecordsInAllFiles()` | Percorre todos os arquivos de log e conta o número total de linhas com mais de 2 caracteres (registros válidos). Retorna o total. Usado pelo `handleSyncProcess()` para montar o pacote SOT. |
| `openLogFileForRead` | `bool openLogFileForRead(const String& filePath)` | Abre o arquivo no caminho especificado em modo leitura e armazena o handle em `logFileBLE`. Retorna `true` em sucesso. |
| `readNextLogEntry` | `String readNextLogEntry()` | Lê e retorna a próxima linha do arquivo aberto por `openLogFileForRead()`. Retorna string vazia se não houver mais conteúdo ou arquivo fechado. |
| `closeLogFile` | `void closeLogFile()` | Fecha o arquivo `logFileBLE` se estiver aberto. |
| `getAllLogFilePaths` | `std::vector<String> getAllLogFilePaths()` | Percorre a estrutura de diretórios em `/logs` e retorna um vetor com os caminhos absolutos de todos os arquivos `.jsonl`. |
| `prepareLogStream` | `void prepareLogStream()` | Prepara o estado interno para um streaming sequencial: limpa `_streamFilePaths`, reseta `_currentStreamFileIndex` e a flag `_isFirstChunk`. Coleta todos os caminhos de arquivo via travessia de diretório. |
| `readLogStreamChunk` | `size_t readLogStreamChunk(uint8_t *buffer, size_t maxLen)` | Lê um chunk de dados do stream de logs para o `buffer`. No primeiro chunk, escreve o `[` de abertura do array JSON. Itera pelos arquivos e linhas, adicionando vírgulas entre elementos. No último chunk, escreve o `]` de fechamento. Retorna o número de bytes escritos no buffer; retorna `0` quando o stream termina. |
| `streamFileJson` *(interno)* | `void streamFileJson(AsyncResponseStream* response, File& file, const String& filename)` | Serializa o conteúdo de um arquivo como objeto JSON com campos `file` e `content` (com escape de caracteres especiais). Escrito diretamente no `AsyncResponseStream`. |

---

## BleHandler

Gerencia toda a pilha BLE do ESP32: servidor GATT, características, callbacks de comandos e protocolo de sincronização com ACK.

#### Configuração e Loop

| Função | Assinatura | Descrição |
|---|---|---|
| `setupBLE` | `void setupBLE(DeviceController& meuDevice)` | Aborta se o `DeviceController` não estiver pronto. Inicializa o dispositivo BLE com o nome `"ESP32_BLE_01"`. Cria o serviço HUB com as características RX (WRITE) e TX (NOTIFY) com seus UUIDs fixos. Cria o serviço de sensores com UUID dinâmico do `HubConfig` e gera uma característica BLE NOTIFY+READ para cada sensor em `meuDevice.getSensors()`, registrando cada uma no `characteristicMap` (sensorId → BLECharacteristic). Inicia ambos os serviços e o advertising. |
| `loopBLE` | `void loopBLE(DeviceController& meuDevice)` | Chamado a cada iteração do `loop()`. Máquina de estados baseada em flags: se `syncRequested=true`, chama `handleSyncProcess()`; se `configRequested=true`, constrói e envia via `sendJsonInChunks()` um JSON com `type:"config"`, os dados do `HubConfig` e o array de sensores serializado por `toConfigJson()`. |

#### Callbacks BLE (internos)

| Classe/Método | Descrição |
|---|---|
| `MyServerCallbacks::onConnect` | Define `deviceConnected = true` e imprime confirmação. |
| `MyServerCallbacks::onDisconnect` | Define `deviceConnected = false`, reseta `syncRequested` e `realTimeStreamActive`, e reinicia o advertising via `BLEDevice::startAdvertising()`. |
| `MyCallbacks::onWrite` | Processa comandos de 1 byte recebidos pela característica RX: `0x01` → ACK; `0x02` → sync; `0x03` → start real-time (notifica todos os sensores imediatamente); `0x05` → stop real-time; `0x06` → delete logs; `0x07` → cancel sync; `0x20` → request config. |

#### Funções de Transmissão

| Função | Assinatura | Descrição |
|---|---|---|
| `notifySensorValue` | `void notifySensorValue(const String& sensor_id, float value, const String& unit)` | Monta um JSON com `sensorId`, `value` e `unit`, adiciona `\n` ao final e notifica a característica BLE do sensor correspondente buscando-a em `characteristicMap`. |
| `sendJsonInChunks` | `void sendJsonInChunks(BLECharacteristic* pChar, const String& json)` | Divide o JSON em fragmentos de 500 bytes e notifica cada fragmento via BLE com um delay de 10 ms entre eles. Garante `\n` no final do JSON completo. |
| `handleSyncProcess` | `void handleSyncProcess()` | Protocolo de sincronização histórica via BLE: (1) conta os registros totais e envia pacote `SOT` com o campo `records`; (2) aguarda ACK via `waitForAck()`; (3) percorre recursivamente `/logs`, lê cada linha e a envia como pacote `data` aguardando ACK após cada uma; (4) envia pacote `EOT` ao final. Aborta se o ACK falhar em qualquer etapa. |
| `waitForAck` | `bool waitForAck()` | Aguarda a flag `ackReceived` ser definida como `true` (pelo callback `onWrite` com byte `0x01`). Timeout de 2 segundos. Retorna `false` se desconectar ou timeout. |
| `printCharacteristicInfo` | `void printCharacteristicInfo(BLECharacteristic* pChar)` | Imprime o UUID da característica no Serial para debug. |

---

## WifiHandler

Configura o ESP32 como Access Point Wi-Fi e serve uma API REST assíncrona via `AsyncWebServer`.

| Função | Assinatura | Descrição |
|---|---|---|
| `setupWiFi` | `void setupWiFi(DeviceController& meuDevice)` | Inicia o modo Access Point com SSID `"ESP32_Sensor_Server"` e registra todos os endpoints no servidor assíncrono. Chama `server.begin()`. |

#### Endpoints HTTP

| Endpoint | Método | Handler | Descrição |
|---|---|---|---|
| `/config` | GET | lambda | Verifica se o `DeviceController` está pronto. Monta JSON com os dados do Hub (`hub_id`, `hub_name`, `latitude`, `longitude`, `min_sampling_interval_ms`) e o array de sensores (via `toConfigJson()`). Responde 200 com o JSON. |
| `/dados` | GET | lambda | Para cada sensor em `meuDevice.getSensors()`, chama `readNow()` e constrói um objeto JSON com `sensorId` e `value` (último valor). Responde 200 com o array JSON de todos os sensores. |
| `/historico` | GET | lambda | Lê o parâmetro `page` (default 1) e delega para `enviarArquivoPorPagina()`. |
| `/limpar_historico` | GET | lambda | Chama `deleteLogFiles()` e responde 200 com `"OK"`. |
| `/info/info` | GET | lambda | Lista todos os arquivos `.jsonl` em `/logs`, retornando para cada um: `pagina`, `nome`, `caminho`, `tamanho` e `modificado`. |

#### Funções Auxiliares do Wi-Fi

| Função | Assinatura | Descrição |
|---|---|---|
| `enviarArquivoPorPagina` | `void enviarArquivoPorPagina(AsyncWebServerRequest* request, int page)` | Coleta todos os arquivos `.jsonl` de `/logs` em um vetor, ordena em ordem decrescente (mais recente primeiro) e mapeia `page` ao arquivo correspondente. Responde 404 se a página não existir. Chama `enviarArquivoInteiro()` para o arquivo selecionado. |
| `enviarArquivoInteiro` | `void enviarArquivoInteiro(AsyncWebServerRequest* request, const String& arquivo, int page, int totalArquivos)` | Abre o arquivo e inicia uma resposta HTTP **chunked** assíncrona. O chunk 1 envia o cabeçalho JSON (`pagina_atual`, `total_arquivos`, `arquivo`, `tamanho`, `linhas:[`). Os chunks intermediários enviam cada linha do arquivo. O chunk final fecha o JSON com `total_linhas`, `proxima_pagina` e `pagina_anterior`. |
| `escapeJSON` | `String escapeJSON(const String& input)` | Escapa caracteres especiais JSON (`"` e `\`) em uma string, prefixando-os com `\`. Usado ao inserir linhas de texto que não são JSON no array de resposta. |
| `lerLinhaDoArquivo` | `String lerLinhaDoArquivo(File& file)` | Lê uma linha de um arquivo com `readStringUntil('\n')` e aplica `trim()` para remover `\r\n`. |
| `isValidJSONLine` | `bool isValidJSONLine(const String& line)` | Verifica se uma linha é válida (comprimento > 0). Implementação simplificada para debug. |
| `resetStreamState` | `void resetStreamState()` | Fecha todos os handles de arquivo do `StreamState` global e reseta a struct para o estado inicial. |
| `getFileNameFromPath` | `String getFileNameFromPath(const String& path)` | Extrai o nome do arquivo de um caminho absoluto, retornando a substring após o último `/`. |

---

## main.cpp

Ponto de entrada do firmware. Contém `setup()`, `loop()` e funções utilitárias de desenvolvimento.

| Função | Assinatura | Descrição |
|---|---|---|
| `setup` | `void setup()` | Inicializa o Serial (115200 baud), I²C, o RTC (`rtcService.begin()` e `adjustToCompileTime()`). Carrega a configuração do Hub (`HubConfig::getInstance().load()`). Inicializa o `DeviceController` (`meuDevice.init()`). Em sucesso, chama `setupDataLogger()`, `setupBLE()` e `setupWiFi()`. Define `isSystemReady`. |
| `loop` | `void loop()` | Se o sistema estiver pronto, itera sobre todos os sensores em `meuDevice.getSensors()` e chama `sensor->update()` em cada um. Chama `loopBLE()` a cada iteração para processar comandos e streaming BLE. |
| `generateTestLogs` | `void generateTestLogs(DeviceController& device)` | **Utilitário de desenvolvimento.** Gera 10 ciclos de leituras simuladas para todos os sensores, usando um timestamp fixo como ponto de partida e incrementando 5 segundos a cada registo. Chama `logSensorReading()` diretamente. |
| `listAllFiles` | `void listAllFiles(const char* basePath, int indent)` | **Utilitário de debug.** Percorre recursivamente o sistema de arquivos a partir de `basePath` e imprime no Serial todos os arquivos e diretórios encontrados com indentação hierárquica. |
| `printJsonlFile` | `void printJsonlFile(const char* filePath)` | **Utilitário de debug.** Abre um arquivo `.jsonl`, lê cada linha, imprime o texto bruto e tenta desserializar o JSON para exibir os campos `ts`, `raw`, `value` e `unit` individualmente. |
