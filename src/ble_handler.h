#ifndef BLE_HANDLER_H
#define BLE_HANDLER_H

#include <Arduino.h>
#include "device_controller.h" // Inclui a sua classe principal

// As funções "públicas" do módulo não mudam
void setupBLE(DeviceController& meuDevice);
void loopBLE(DeviceController& meuDevice);
void notifySensorValue(const String& sensor_id, float value);
void sendMainTxPacket(const String& jsonPacket);

#endif