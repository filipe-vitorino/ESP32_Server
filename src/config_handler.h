#ifndef CONFIG_HANDLER_H
#define CONFIG_HANDLER_H

#include <Arduino.h>

void setupConfig();
String getConfigJsonString(); // Função para ler e retornar o JSON como uma String

#endif