/*
 * rlvl1_defins.h
 *
 *  Created on: Nov 21, 2013
 *      Author: kreyl
 */

#ifndef RLVL1_DEFINS_H_
#define RLVL1_DEFINS_H_

#if 1 // ========================= Signal levels ===============================
// pyton translation for db
#define RX_LVL_TOP      1000
// Jolaf: str(tuple(1 + int(sqrt(float(i) / 65) * 99) for i in xrange(0, 65 + 1)))
//const int32_t dBm2Percent1000Tbl[66] = {10, 130, 180, 220, 250, 280, 310, 330, 350, 370, 390, 410, 430, 450, 460, 480, 500, 510, 530, 540, 550, 570, 580, 590, 610, 620, 630, 640, 650, 670, 680, 690, 700, 710, 720, 730, 740, 750, 760, 770, 780, 790, 800, 810, 820, 830, 840, 850, 860, 860, 870, 880, 890, 900, 910, 920, 920, 930, 940, 950, 960, 960, 970, 980, 990, 1000};
const int32_t dBm2Percent1000Tbl[86] = {
         10, 117, 162, 196, 225, 250, 273, 294, 314, 332,
        350, 366, 382, 397, 412, 426, 440, 453, 466, 478,
        490, 502, 514, 525, 536, 547, 558, 568, 578, 588,
        598, 608, 617, 627, 636, 645, 654, 663, 672, 681,
        689, 698, 706, 714, 722, 730, 738, 746, 754, 762,
        769, 777, 784, 792, 799, 806, 814, 821, 828, 835,
        842, 849, 856, 862, 869, 876, 882, 889, 895, 902,
        908, 915, 921, 927, 934, 940, 946, 952, 958, 964,
        970, 976, 982, 988, 994, 1000
};

static inline int32_t dBm2Percent(int32_t Rssi) {
    if(Rssi < -100) Rssi = -100;
    else if(Rssi > -15) Rssi = -15;
    Rssi += 100;    // 0...85
    return dBm2Percent1000Tbl[Rssi];
}

// Conversion Lvl1000 <=> Lvl250
#define Lvl1000ToLvl250(Lvl1000) ((uint8_t)((Lvl1000 + 3) / 4))

static inline void Lvl250ToLvl1000(uint16_t *PLvl) {
    *PLvl = (*PLvl) * 4;
}

// Sensitivity Constants, percent [1...1000]. Feel if RxLevel > SnsConst.
#define RLVL_NEVER              10000
#define RLVL_2M                 800     // 0...4m
#define RLVL_4M                 700     // 1...20m
#define RLVL_10M                600
#define RLVL_50M                1
#define RLVL_PELENGATOR         RLVL_4M // LED will lit if rlevel is higher

#endif

#if 1 // =========================== Pkt_t =====================================
struct rPkt_t {
    uint8_t LvlMin;
    uint8_t LvlMax;
    uint8_t DmgMin;
    uint8_t DmgMax;
} __attribute__ ((__packed__));
#define RPKT_LEN    sizeof(rPkt_t)
#endif

#if 1 // ========================== const Pkt_t ================================
// Lustra constants
const rPkt_t PktLustra[4] = {
        {Lvl1000ToLvl250(650), Lvl1000ToLvl250(1000), 0, 0},        // Clean
        {Lvl1000ToLvl250(650), Lvl1000ToLvl250(1000), 1, 3}, // DEBUG 11},       // Weak: 11^2 = 121
        {Lvl1000ToLvl250(650), Lvl1000ToLvl250(1000), 1, 42},       // Strong: 42^2 = 1800
        {Lvl1000ToLvl250(650), Lvl1000ToLvl250(1000), 0xFF, 0xFF},  // Lethal: DoseTop
};
// Pelengator, Grenade
const rPkt_t PktDummy = {1, 2, 3, 4};
#endif

#if 1 // ======================= Channels & cycles =============================
#define LUSTRA_CNT      40
#define LUSTRA_MIN_ID   100
#define LUSTRA_MAX_ID   140

#define RCHNL_EMP       7
#define RCHNL_PELENG    9
#define RCHNL_MIN       10
#define RCHNL_MAX       (RCHNL_MIN + LUSTRA_CNT - 1)

#define LUSTRA_ID_TO_RCHNL(ID) ((ID - LUSTRA_MIN_ID) + RCHNL_MIN)

#define CYCLE_CNT       4   // Number of cycles in supercycle
#define PELENG_TX_CNT   4   // Number of pkts to transmit by pelengator

#endif

#if 1 // =========================== Timings ===================================
#define PELENG_RX_T_MS  4   // }
#define LUSTRA_RX_T_MS  5   // } Experimentally checked

#endif




#endif /* RLVL1_DEFINS_H_ */
