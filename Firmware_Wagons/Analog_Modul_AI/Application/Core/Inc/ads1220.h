#ifndef ADS1220_H
#define ADS1220_H

#include "main.h"

// Initialize the ADS1220 (PGA bypass, Gain 1, internal 2.048V ref, 20 SPS, 50/60 Hz rejection)
void ADS1220_Init(void);

// Single-shot conversion of the given multiplexer channel.
// mux_channel: e.g. 0x08 for AIN0 single-ended, 0x09 for AIN1 single-ended
// Returns HAL_TIMEOUT if DRDY did not go low; *voltage is then left unchanged.
HAL_StatusTypeDef ADS1220_ReadVoltage(uint8_t mux_channel, float *voltage);

#endif // ADS1220_H
