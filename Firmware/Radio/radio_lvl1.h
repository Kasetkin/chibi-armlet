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
    uint8_t PowerLvlId;
//    bool operator == (const rPkt_t &APkt) { return (DWord32 == APkt.DWord32); }
//    rPkt_t& operator = (const rPkt_t &Right) {
//        chSysLock();
//        ID = Right.ID;
//        LvlMin = Right.LvlMin;
//        LvlMax = Right.LvlMax;
//        DmgMin = Right.DmgMin;
//        DmgMax = Right.DmgMax;
//        chSysUnlock();
//        return *this;
//    }
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

class rLevel1_t {
public:
    uint8_t Init();
    rPkt_t PktRx;
    // Inner use
    void TryToSleep(uint32_t SleepDuration);
    void TryToReceive(uint32_t RxDuration);
};

extern rLevel1_t Radio;
