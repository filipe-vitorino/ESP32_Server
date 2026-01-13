#ifndef TDS_SENSOR_H
#define TDS_SENSOR_H

#include "BaseSensor.h" // Inclui a definição da classe mãe
#include <vector>
// SoilMoistureSensor "é um" Sensor
class TdsSensor : public Sensor {
public:
    /**
     * @brief Construtor. Recebe o pino analógico onde o sensor está conectado.
     * @param pin O número do pino (ex: 34).
     */
    TdsSensor(uint8_t pin);
    
    /**
     * @brief Implementação do método que lê e calcula o valor de umidade.
     * @return O valor da umidade em percentual (ou na faixa definida na calibração).
     */
    float getValue(int rawValue) override;
    int getRaw() override;

   

protected:
    /**
     * @brief Implementação do método que configura os parâmetros de calibração
     * específicos para este tipo de sensor.
     * @param calibrationConfig O objeto JSON "calibration" do arquivo de configuração.
     */
    void _configureCalibration(const JsonVariant& calibrationConfig) override;

private:
    // --- Membros Privados Específicos Deste Sensor ---

    // Pino onde o sensor está conectado
    uint8_t _pin;

    String _calibrationType;
    float _a, _b;                     // linear
    std::vector<float> _coefficients; // polynomial
    float _factor;
    float _rangeMin;    // Valor mínimo da faixa de saída (ex: 0)
    float _rangeMax;    // Valor máximo da faixa de saída (ex: 100)
};

#endif