/*
 * radio_lvl1.cpp
 *
 *  Created on: Nov 17, 2013
 *      Author: kreyl
 */

#include "radio_lvl1.h"
#include "cc1101.h"
#include "uart.h"
#include "main.h"

#include "led.h"
#include "Sequences.h"
#include <type_traits>

extern LedRGBwPower_t Led;

cc1101_t CC(CC_Setup0);

//#define DBG_PINS

#ifdef DBG_PINS
#define DBG_GPIO1   GPIOB
#define DBG_PIN1    10
#define DBG1_SET()  PinSetHi(DBG_GPIO1, DBG_PIN1)
#define DBG1_CLR()  PinSetLo(DBG_GPIO1, DBG_PIN1)
#define DBG_GPIO2   GPIOB
#define DBG_PIN2    9
#define DBG2_SET()  PinSetHi(DBG_GPIO2, DBG_PIN2)
#define DBG2_CLR()  PinSetLo(DBG_GPIO2, DBG_PIN2)
#else
#define DBG1_SET()
#define DBG1_CLR()
#endif

rLevel1_t Radio;

bool RxData_t::ProcessAndCheck()
{
	bool result = false;

	if (Cnt >= 3L) {
		Summ /= Cnt;
		if(Summ >= Threshold)
			result = true;
	}

	Cnt = 0;
	Summ = 0;

	return result;
}

#if 1 // ================================ Task =================================
static THD_WORKING_AREA(warLvl1Thread, 256);
__noreturn
static void rLvl1Thread(void *arg) {
    chRegSetThreadName("rLvl1");
    while(true) {
        int8_t Rssi;
        rPkt_t RxPkt;
        CC.Recalibrate();
        uint8_t RxRslt = CC.Receive(360, &RxPkt, RPKT_LEN, &Rssi);
        if(RxRslt == retvOk) {
            Printf("%u: Thr: %d; Dmg: %u; Rssi: %d\r", RxPkt.From, RxPkt.RssiThr, RxPkt.Value, Rssi);
            // Damage pkt from lustra
            if(RxPkt.From >= LUSTRA_MIN_ID and RxPkt.From <= LUSTRA_MAX_ID) {
                // Add to accumulator. Averaging is done in main thd
                int32_t Indx = RxPkt.From - LUSTRA_MIN_ID;
                Radio.RxData[Indx].Cnt++;
                Radio.RxData[Indx].Summ += Rssi;
                Radio.RxData[Indx].Threshold = RxPkt.RssiThr;
                Radio.RxData[Indx].Dmg = RxPkt.Value;
            } else if (RxPkt.From == DIAGNOSTIC_SENDER_ID) {
            	Radio.diagCmd = static_cast<DiagnosticCommand>(RxPkt.Value);
            	if (RxPkt.Value != static_cast<int8_t>(Radio.diagCmd))
            		Radio.diagCmd = DiagnosticCommand::none;
            }
        }
    } // while true
}
#endif // task

//void rLevel1_t::TryToSleep(uint32_t SleepDuration) {
//    if(SleepDuration >= MIN_SLEEP_DURATION_MS) CC.EnterPwrDown();
//    chThdSleepMilliseconds(SleepDuration);
//}

#if 1 // ============================
uint8_t rLevel1_t::Init() {
#ifdef DBG_PINS
    PinSetupOut(DBG_GPIO1, DBG_PIN1, omPushPull);
    PinSetupOut(DBG_GPIO2, DBG_PIN2, omPushPull);
#endif

    for(int i=0; i<LUSTRA_CNT; i++) {
        RxData[i].Cnt = 0;
        RxData[i].Summ = 0;
    }
    diagCmd = DiagnosticCommand::none;
    RMsgQ.Init();
    if(CC.Init() == retvOk) {
        CC.SetTxPower(CC_PwrMinus20dBm);
        CC.SetPktSize(RPKT_LEN);
        CC.SetChannel(0);
//        CC.EnterPwrDown();
        // Thread
        chThdCreateStatic(warLvl1Thread, sizeof(warLvl1Thread), HIGHPRIO, (tfunc_t)rLvl1Thread, NULL);
        return retvOk;
    }
    else return retvFail;
}
#endif
