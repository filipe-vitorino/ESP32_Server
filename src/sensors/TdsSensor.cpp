#include "TdsSensor.h"

/**
 * @brief Construtor: é executado quando o objeto é criado com 'new'.
 * Usa a lista de inicialização para definir o pino e valores padrão de segurança.
 */
TdsSensor::TdsSensor(uint8_t pin) : _pin(pin),
      _rangeMin(-50.0f), _rangeMax(125.0f){

}

/**
 * @brief Lê o objeto JSON de calibração e armazena os valores nos membros privados.
 */
void TdsSensor::_configureCalibration(const JsonVariant& calibrationConfig) {
   
   serializeJsonPretty(calibrationConfig["valid_range"]["min"], Serial);
   float x = calibrationConfig["valid_range"]["min"];
   float y = calibrationConfig["valid_range"]["max"];
   Serial.println();
   Serial.println(x); 
   Serial.println(y);
   
    _calibrationType = calibrationConfig["type"] | "linear";

    // --- Polynomial ---
    if (_calibrationType == "polynomial") {
        // CORREÇÃO: Usamos JsonArrayConst para ler de um objeto 'const'
        JsonArrayConst coeffs = calibrationConfig["coefficients"].as<JsonArrayConst>();
        
        if (coeffs) {
            _coefficients.clear();
            for (JsonVariantConst v : coeffs) { // O 'v' também se torna const
                _coefficients.push_back(v.as<float>());
            }
        }
        _factor = calibrationConfig["factor"] | 1.0;
    }

    // --- Linear (Exemplo corrigido para o futuro) ---
    else if (_calibrationType == "linear") {
        // CORREÇÃO: Usamos JsonObjectConst
        JsonObjectConst coeffs = calibrationConfig["coefficients"].as<JsonObjectConst>();
        if (coeffs) {
            _a = coeffs["a"] | 1.0;
            _b = coeffs["b"] | 0.0;
        }
    }

    //_rangeMin = calibrationConfig["valid_range"]["min"] | 55.0f;
    //_rangeMax = calibrationConfig["valid_range"]["max"] | 5500.0f;
    // Inicializa valores padrão
        _rangeMin = x;
        _rangeMax = y;


Serial.printf("-> TDS Faixa final: min=%.2f max=%.2f\n", _rangeMin, _rangeMax);

        Serial.printf("-> TDS Faixa final: min=%.2f max=%.2f\n", _rangeMin, _rangeMax);


    Serial.printf("   -> Calibração TDS (ID: %s) carregada: tipo=%s, coef[%d], range=[%.2f, %.2f]\n",
                  _sensor_id.c_str(),
                  _calibrationType.c_str(),
                  _coefficients.size(),
                  _rangeMin,
                  _rangeMax);
}

int TdsSensor::getRaw(){
    return analogRead(_pin);
}
/**
 * @brief Contém a lógica de hardware: ler o pino e aplicar a matemática.
 */
float TdsSensor::getValue(int rawValue) {
    float value = 0;

    if (_calibrationType == "linear") {
        value = _a * rawValue + _b;
    } 
    else if (_calibrationType == "polynomial") {
        int degree = _coefficients.size() - 1;
        for (int i = 0; i <= degree; i++) {
            value += _coefficients[i] * pow(rawValue, degree - i);
        }
        value *= _factor;
    } 
    else {
        value = rawValue; // fallback
    }
    value = random(0, 100)/10;
    // Limita ao intervalo válido
    return constrain(value, _rangeMin, _rangeMax);
}


