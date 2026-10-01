#include "ads1220.h"
#include "spi.h"

#define ADS1220_CMD_RESET   0x06
#define ADS1220_CMD_START   0x08
#define ADS1220_CMD_RDATA   0x10
#define ADS1220_CMD_WREG    0x40

extern SPI_HandleTypeDef hspi1;

static void ADS1220_WriteRegister(uint8_t reg, uint8_t data)
{
    uint8_t cmd[2];
    cmd[0] = ADS1220_CMD_WREG | (reg << 2);
    cmd[1] = data;

    HAL_GPIO_WritePin(STM_CS_GPIO_Port, STM_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, cmd, 2, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(STM_CS_GPIO_Port, STM_CS_Pin, GPIO_PIN_SET);
}

void ADS1220_Init(void)
{
    uint8_t reset_cmd = ADS1220_CMD_RESET;

    HAL_GPIO_WritePin(STM_CS_GPIO_Port, STM_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, &reset_cmd, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(STM_CS_GPIO_Port, STM_CS_Pin, GPIO_PIN_SET);
    
    HAL_Delay(5);

    // Reg 1: Single-shot mode, 20 SPS
    ADS1220_WriteRegister(1, 0x00);
    // Reg 2: Internal 2.048V ref, simultaneous 50/60 Hz rejection (valid at 20 SPS only)
    ADS1220_WriteRegister(2, 0x10);
    // Reg 3: Defaults
    ADS1220_WriteRegister(3, 0x00);
}

HAL_StatusTypeDef ADS1220_ReadVoltage(uint8_t mux_channel, float *voltage)
{
    // Reg 0: MUX channel, Gain 1, PGA bypass
    // PGA bypass = bit 0 (1 = bypassed)
    // Gain 1 = bits 3-1 (000)
    uint8_t reg0 = (mux_channel << 4) | 0x01;
    ADS1220_WriteRegister(0, reg0);

    // Send START command
    uint8_t start_cmd = ADS1220_CMD_START;
    HAL_GPIO_WritePin(STM_CS_GPIO_Port, STM_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, &start_cmd, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(STM_CS_GPIO_Port, STM_CS_Pin, GPIO_PIN_SET);

    // Wait for DRDY to go low
    // Assuming DRDY pin is defined and we can read it. If it's an interrupt, 
    // for this blocking function we just poll it.
    uint32_t start = HAL_GetTick();
    while(HAL_GPIO_ReadPin(STM_DRDY_GPIO_Port, STM_DRDY_Pin) == GPIO_PIN_SET)
    {
        if(HAL_GetTick() - start > 100) return HAL_TIMEOUT;
    }

    // Read Data
    uint8_t rdata_cmd = ADS1220_CMD_RDATA;
    uint8_t rx_data[3] = {0};

    HAL_GPIO_WritePin(STM_CS_GPIO_Port, STM_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, &rdata_cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi1, rx_data, 3, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(STM_CS_GPIO_Port, STM_CS_Pin, GPIO_PIN_SET);

    int32_t adc_code = (rx_data[0] << 16) | (rx_data[1] << 8) | rx_data[2];
    
    // Sign extension for 24-bit
    if(adc_code & 0x800000) {
        adc_code |= 0xFF000000;
    }

    // Convert code to voltage. Internal Vref = 2.048V. 
    // 1 LSB = (2 * Vref / Gain) / 2^24 = (2 * 2.048) / 16777216 = 2.048 / 8388608
    *voltage = (float)adc_code * (2.048f / 8388608.0f);

    return HAL_OK;
}
