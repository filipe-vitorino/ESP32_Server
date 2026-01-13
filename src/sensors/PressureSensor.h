#ifndef PRESSURE_SENSOR_H
#define PRESSURE_SENSOR_H

#include "BaseSensor.h" // Corrigido para "BaseSensor" (ou "Sensor", o que for o nome do seu arquivo base)

class PressureSensor : public Sensor { // Corrigido para "BaseSensor"
public:
    /**
     * @brief Construtor. Recebe o pino analógico onde o sensor está conectado.
     */
    PressureSensor(uint8_t pin);
    
    /**
     * @brief Implementação do método que lê e calcula o valor da PRESSÃO.
     */
    float getValue(int rawValue) override;

protected:
    /**
     * @brief Configura os parâmetros de calibração para o sensor de pressão.
     */
    void _configureCalibration(const JsonVariant& calibrationConfig) override;

    int getRaw() override;

private:
    uint8_t _pin;

    // --- Usando os nomes de variáveis que você definiu ---
    int _zero;
    int _cem;
    int _rangeMin;
    int _rangeMax;
};

#endif // PRESSAO_SENSOR_H