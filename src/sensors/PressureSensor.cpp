#include "PressureSensor.h"

/**
 * @brief Construtor: é executado quando o objeto é criado com 'new'.
 * Usa a lista de inicialização para definir o pino e valores padrão de segurança.
 */
PressureSensor::PressureSensor(uint8_t pin) : _pin(pin) {
    // Inicializa com valores padrão seguros.
    // Estes serão substituídos pelos valores do JSON quando .configure() for chamado.
    _zero = 0;
    _cem = 4095; // Valor máximo para o ADC de 12-bit do ESP32
    _rangeMin = 0;
    _rangeMax = 100;
}

/**
 * @brief Lê o objeto JSON de calibração e armazena os valores nos membros privados.
 */
void PressureSensor::_configureCalibration(const JsonVariant& calibrationConfig) {
    // Usamos o operador '|' da ArduinoJson para definir um valor padrão
    // caso a chave não exista no arquivo JSON. Isso torna o sistema mais robusto.
    _zero = calibrationConfig["low_pressure_value"] | 0;
    _cem = calibrationConfig["high_pressure_value"] | 4095;
    
    // Acessa o objeto aninhado "valid_range"
    _rangeMin = calibrationConfig["valid_range"]["min"] | 10;
    _rangeMax = calibrationConfig["valid_range"]["max"] | 100;

    Serial.printf("  -> Calibração do Pressao (ID: %s) carregada: Seco=%d, Úmido=%d\n",
                  _sensor_id.c_str(), _rangeMin, _rangeMax);
}

int PressureSensor::getRaw(){
    return analogRead(_pin);
}
/**
 * @brief Contém a lógica de hardware: ler o pino e aplicar a matemática.
 */
float PressureSensor::getValue(int rawValue) {
    // 1. Lê o valor analógico bruto (0-4095 no ESP32)
    //int rawValue = analogRead(_pin);

    // 2. Garante que o valor lido esteja dentro dos limites da calibração para evitar resultados estranhos
    rawValue = constrain(rawValue, _zero, _cem);

    // 3. Mapeia a faixa de leitura (ex: 2750-1149) para a faixa de saída (ex: 0-100)
    // Note que _dryValue vem primeiro, pois corresponde ao valor mínimo (0%)
    float mappedValue = map(rawValue, _zero, _cem, _rangeMin, _rangeMax);
    
    mappedValue = random(0, 100)/10;
    return mappedValue;
}

