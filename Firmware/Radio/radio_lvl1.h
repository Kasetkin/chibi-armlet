/*
 * radio_lvl1.h
 *
 *  Created on: Nov 17, 2013
 *      Author: kreyl
 */

#pragma once

#include "kl_lib.h"
#include "ch.h"
#include "cc1101.h"
#include "kl_buf.h"
#include "uart.h"
#include "color.h"

#if 1 // =========================== Pkt_t =====================================
struct rPkt_t {
    uint16_t From;
    uint16_t To;
    int8_t RssiThr;
    uint16_t Value;
} __attribute__ ((__packed__));
#endif


#if 1 // ============================== Defines ================================
#define RCHNL                   0
#define RPKT_LEN                sizeof(rPkt_t)
#define CC_TX_PWR               CC_Pwr0dBm

#define CMD_GET                 18
#define CMD_SET                 27
#define CMD_FLASH               36
#define CMD_OK                  0

#define RX_T_MS                 99
#define RX_SLEEP_T_MS           810

#endif


const uint8_t PwrTable[12] = {
        CC_PwrMinus30dBm, // 0
        CC_PwrMinus27dBm, // 1
        CC_PwrMinus25dBm, // 2
        CC_PwrMinus20dBm, // 3
        CC_PwrMinus15dBm, // 4
        CC_PwrMinus10dBm, // 5
        CC_PwrMinus6dBm,  // 6
        CC_Pwr0dBm,       // 7
        CC_PwrPlus5dBm,   // 8
        CC_PwrPlus7dBm,   // 9
        CC_PwrPlus10dBm,  // 10
        CC_PwrPlus12dBm   // 11
};

class rLevel1_t {
public:
    uint8_t Init();
    rPkt_t PktRx;
    // Inner use
    void TryToSleep(uint32_t SleepDuration);
    void TryToReceive(uint32_t RxDuration);
};

extern rLevel1_t Radio;
