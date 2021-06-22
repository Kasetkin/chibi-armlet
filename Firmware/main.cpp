#include "kl_lib.h"
#include "ch.h"
#include "hal.h"
#include "uart.h"
#include "radio_lvl1.h"
#include "beeper.h"
#include "led.h"
#include "Sequences.h"
#include "main.h"
#include "pill.h"
#include "pill_mgr.h"
#include "kl_i2c.h"

#if 1 // ======================== Variables and defines ========================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};

__unused
static const uint8_t PwrTable[12] = {
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

int32_t ID = 1;
uint32_t DEST = 0; // to everyone
int32_t RssiThr = -99;
uint16_t Damage = 1;

// EEAddresses
#define EE_ADDR_ID          0
#define EE_ADDR_THRESHOLD   4
#define EE_ADDR_DAMAGE      8

LedRGB_t Led { LED_R_PIN, LED_G_PIN, LED_B_PIN };
Beeper_t Beeper {BEEPER_PIN};

cc1101_t CC(CC_Setup0);

void OnCmd(Cmd_t *PCmd);

void ReadParams();
uint8_t SetID(int32_t NewID);
uint8_t SetThr(int32_t NewThr);
uint8_t SetDmg(uint32_t NewDmg);
#endif

int main(void) {
    Iwdg::InitAndStart(198);
    // ==== Init Vcore & clock system ====
    SetupVCore(vcore1V5);
    Clk.SetMSI4MHz();
    Clk.UpdateFreqValues();
    // ==== Init OS ====
    halInit();
    chSysInit();

    // ==== Init Hard & Soft ====
    PinSetupOut(GPIOC, 15, omPushPull); // To measure total ON time
    PinSetHi(GPIOC, 15);
    PinSetupOut(GPIOC, 14, omPushPull); // To measure TX time
    Uart.Init();

    ReadParams();
//   /// Diagnostic params:
//  constexpr uint16_t DIAGNOSTIC_CLIENT_ID = 2000;
//  constexpr uint16_t DIAGNOSTIC_AUTOSERVER_ID = 2001;
//	constexpr uint16_t answerFromDiagServer = 3003;

/// lustra test parameters
    ID = 1099;
    RssiThr = -40;
    Damage = 1000;

    if(!Sleep::WasInStandby()) {
        Led.Init();
        Led.StartOrRestart(lsqStart0);
        Printf("\r%S %S\rID=%u; Thr=%d; Dmg=%u\r", APP_NAME, XSTRINGIFY(BUILD_TIME), ID, RssiThr, Damage);
        Clk.PrintFreqs();
        for(int i=0; i<27; i++) {
            chThdSleepMilliseconds(99);
            Iwdg::Reload();
            uint8_t b;
            while(Uart.GetByte(&b) == retvOk) {
                if(Uart.Cmd.PutChar(b) == pdrNewCmd) {
                    OnCmd(&Uart.Cmd);
                    i = 0;
                }
            } // while get byte
        } // for
    }

    if(CC.Init() == retvOk) {
        if(!Sleep::WasInStandby()) {
            Led.StartOrRestart(lsqStart);
            chThdSleepMilliseconds(126);
            Iwdg::Reload();
            chThdSleepMilliseconds(126);
            Iwdg::Reload();
        }
        // Setup CC
        CC.SetTxPower(CC_PwrPlus5dBm);
        CC.SetPktSize(RPKT_LEN);
        CC.SetChannel(0);
        // Transmit
        rPkt_t Pkt;
        Pkt.From = ID;
        Pkt.To = DEST; // to everyone
        Pkt.RssiThr = RssiThr;
        Pkt.Value = Damage;
        PinSetHi(GPIOC, 14);
        CC.Recalibrate();
        CC.Transmit(&Pkt, RPKT_LEN);
        PinSetLo(GPIOC, 14);
        Iwdg::Reload();
        // Receive
        if(CC.Receive(4, &Pkt, RPKT_LEN) == retvOk) {
//            Printf("%u: Thr: %d; Pwr: %u\r", Pkt.From, Pkt.RssiThr, Pkt.PowerLvlId);
//            chThdSleepMilliseconds(9);
            if(Pkt.From == 1 and Pkt.To == ID) { // From host to me
                if(Pkt.RssiThr > 0) {
                    Led.Init();
                    Beeper.Init();
                    Beeper.StartOrRestart(bsqSearch);
                    Led.StartOrRestart(lsqSearch);
                    chThdSleepMilliseconds(108);
                }
                else {
                    SetThr(Pkt.RssiThr);
                    SetDmg(Pkt.Value);
                }
            }
        }
        CC.EnterPwrDown();
    }
    else {
        Led.Init();
        Led.StartOrRestart(lsqFailure);
        chThdSleepMilliseconds(126);
        Iwdg::Reload();
        chThdSleepMilliseconds(126);
        Iwdg::Reload();
        chThdSleepMilliseconds(126);
        Iwdg::Reload();
        chThdSleepMilliseconds(126);
        Iwdg::Reload();
        chThdSleepMilliseconds(126);
        Iwdg::Reload();
        chThdSleepMilliseconds(126);
        Iwdg::Reload();
    }

    chSysLock();
    Iwdg::Reload();
    Sleep::EnterStandby();
    chSysUnlock();

    while(true); // Will never be here
}

#if 1 // =========================== Cmd handling ==============================
void Ack(int32_t Result) { Printf("Ack %d\r\n", Result); }

void OnCmd(Cmd_t *PCmd) {
    if(PCmd->NameIs("Ping")) {
        Ack(retvOk);
    }
    else if(PCmd->NameIs("GetID")) Printf("ID %u\r", ID);

    else if(PCmd->NameIs("SetID")) {
        int32_t NewID;
        if(PCmd->GetNext<int32_t>(&NewID) != retvOk) { Ack(retvCmdError); return; }
        Ack(SetID(NewID));
    }

    else if(PCmd->NameIs("GetThr")) Printf("Thr %d\r", RssiThr);

    else if(PCmd->NameIs("SetThr")) {
        int32_t NewThr;
        if(PCmd->GetNext<int32_t>(&NewThr) != retvOk) { Ack(retvCmdError); return; }
        Ack(SetThr(NewThr));
    }

    else if(PCmd->NameIs("SetDmg")) {
        uint32_t NewDmg;
        if(PCmd->GetNext<uint32_t>(&NewDmg) != retvOk) { Ack(retvCmdError); return; }
        Ack(SetDmg(NewDmg));
    }

    else if(PCmd->NameIs("Set")) {
        int32_t NewID;
        if(PCmd->GetNext<int32_t>(&NewID) != retvOk) { Ack(retvCmdError); return; }
        int32_t NewThr;
        if(PCmd->GetNext<int32_t>(&NewThr) != retvOk) { Ack(retvCmdError); return; }
        uint32_t NewDmg;
        if(PCmd->GetNext<uint32_t>(&NewDmg) != retvOk) { Ack(retvCmdError); return; }
        Printf("Ack %u %u %u\r\n", SetID(NewID), SetThr(NewThr), SetDmg(NewDmg));
    }
}
#endif

#if 1 // ============================ Save/Load Params =========================
void ReadParams() {
    ID = EE::Read32(EE_ADDR_ID);
    RssiThr = EE::Read32(EE_ADDR_THRESHOLD);
    Damage = EE::Read32(EE_ADDR_DAMAGE);
}

uint8_t SetID(int32_t NewID) {
    uint8_t rslt = EE::Write32(EE_ADDR_ID, NewID);
    if(rslt == retvOk) {
        ID = NewID;
        return retvOk;
    }
    else {
        Printf("EE error: %u\r", rslt);
        return retvFail;
    }
}

uint8_t SetThr(int32_t NewThr) {
    uint8_t rslt = EE::Write32(EE_ADDR_THRESHOLD, NewThr);
    if(rslt == retvOk) {
        RssiThr = NewThr;
        return retvOk;
    }
    else {
        Printf("EE error: %u\r", rslt);
        return retvFail;
    }
}

uint8_t SetDmg(uint32_t NewDmg) {
    uint8_t rslt = EE::Write32(EE_ADDR_DAMAGE, NewDmg);
    if(rslt == retvOk) {
        Damage = NewDmg;
        return retvOk;
    }
    else {
        Printf("EE error: %u\r", rslt);
        return retvFail;
    }
}
#endif
