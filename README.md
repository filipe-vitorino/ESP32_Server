# PAD ESP32 Firmware

Firmware desenvolvido para **ESP32** utilizando **PlatformIO** (VS Code), responsável pela leitura de sensores e comunicação com o aplicativo mobile via **Bluetooth Low Energy (BLE)** e **Wi-Fi**.

Este projeto integra a solução de monitoramento do **Programa Água Doce**, permitindo coleta local, transmissão em tempo real e armazenamento de histórico.

---

## Funcionalidades

- Leitura de sensores conectados ao ESP32
- Comunicação via Bluetooth Low Energy (BLE)
- API local via Wi-Fi
- Registro histórico em memória local
- Sincronização com aplicativo Flutter
- Configuração dinâmica por arquivos JSON
- Arquitetura modular em C++

---

## Sensores Suportados

- Vazão
- Volume
- Temperatura
- Pressão
- TDS (Total Dissolved Solids)

---

## Tecnologias Utilizadas

- ESP32
- C++
- Arduino Framework
- PlatformIO
- Bluetooth Low Energy
- Wi-Fi
- LittleFS
- JSON (ArduinoJson)

---

## Requisitos

Antes de rodar o projeto, instale:

- VS Code
- Extensão PlatformIO IDE
- Python (normalmente instalado junto ao PlatformIO)
- Driver USB da placa ESP32
- Cabo USB de dados

---

## Como Executar o Projeto

### 1. Clonar repositório

```bash
git clone https://github.com/SEU-USUARIO/SEU-REPOSITORIO.git
cd SEU-REPOSITORIO
```

### 2. Abrir no VS Code

Abra a pasta do projeto no VS Code com PlatformIO instalado.

### 3. Instalar dependências

O PlatformIO baixa automaticamente ao abrir o projeto.

### 4. Compilar projeto

```bash
pio run
```

### 5. Enviar para ESP32

```bash
pio run --target upload
```

### 6. Monitor serial

```bash
pio device monitor
```

---

## Estrutura do Projeto

```text
src/
 ├── main.cpp
 ├── sensors/
 ├── services/
 ├── ble/
 └── wifi/

include/
data/
platformio.ini
```

---

## Arquivo platformio.ini

Exemplo básico:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
```

---

## Sistema de Arquivos

O projeto utiliza **LittleFS** para armazenar:

- Configurações do hub
- Configurações de sensores
- Logs históricos

Para enviar arquivos da pasta `data/`:

```bash
pio run --target uploadfs
```

---

## Comunicação

### Bluetooth BLE

Utilizado para:

- Streaming em tempo real
- Sincronização histórica
- Configuração do dispositivo

### Wi-Fi

Utilizado para:

- Endpoint `/config`
- Endpoint `/dados`
- Endpoint `/historico`

---

## Possíveis Problemas

### Porta COM não encontrada

```bash
pio device list
```

### Erro ao enviar firmware

- Verifique cabo USB
- Pressione botão BOOT ao gravar
- Feche monitor serial

### Dependências quebradas

```bash
pio run -t clean
pio run
```

---

## Objetivo Acadêmico

Projeto embarcado desenvolvido com foco em IoT aplicada ao monitoramento hídrico, automação e aquisição inteligente de dados.

---

## Autor

Filipe Vitorino
