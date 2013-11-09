/*
 * pill.h
 *
 *  Created on: Apr 17, 2013
 *      Author: g.kruglov
 */

#ifndef PILL_H_
#define PILL_H_

#include "cmd_uart.h"
#include "ch.h"

#define EEADDR              0x50    // Standard address start of EEPROM - 0x01010aaa
#define PILL_START_ADDR     0x00    // Address of data, common for all
#define PILL_CNT            1       // Number of simultaneously connected pills
// Number of bytes to be written simultaneously. IC dependant, see datasheet.
#define PILL_PAGE_SZ        8

// I2C & hardware
#define PERIPH_PWR_GPIO     GPIOB
#define PERIPH_PWR_PIN      7
#define PILL_I2C_GPIO       GPIOB
#define PILL_SCL_PIN        8
#define PILL_SDA_PIN        9

#define PILL_I2C            I2C1
#define PILL_I2C_BITRATE_HZ 200000
#define PILL_DMATX          STM32_DMA1_STREAM6
#define PILL_DMARX          STM32_DMA1_STREAM7

class Pill_t {
private:
    uint8_t IAddr;
public:
    bool Connected;
    void Init(uint8_t IcAddr);
    uint8_t CheckIfConnected();
    uint8_t Read(uint8_t *Ptr, uint8_t Length);
    uint8_t Write(uint8_t *Ptr, uint8_t Length);
};

void PillInit();
void PillChecker();

extern bool PillsHaveChanged;

extern Pill_t Pill[PILL_CNT];


#endif /* PILL_H_ */
