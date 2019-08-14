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

rPkt_t PktTx, PktRx;

#define MESH_PWR_LVL_ID     8   // 5 dBm

int32_t ID = 1;
int32_t RssiThr = -99;
uint8_t PwrLvlId = 7; // 0dBm
uint8_t Damage = 1;

// EEAddresses
#define EE_ADDR_ID          0
#define EE_ADDR_THRESHOLD   4
#define EE_ADDR_PWR_LVL_ID  8
#define EE_ADDR_DAMAGE_ID   12

LedRGB_t Led { LED_R_PIN, LED_G_PIN, LED_B_PIN };
Beeper_t Beeper {BEEPER_PIN};

cc1101_t CC(CC_Setup0);

void OnCmd(Cmd_t *PCmd);

void ReadParams();
uint8_t SetID(int32_t NewID);
uint8_t SetThr(int32_t NewThr);
uint8_t SetPwr(uint32_t NewPwrId);

void MeshTask();
void BeaconTask();
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
    PinSetupOut(GPIOC, 15, omPushPull);
    PinSetHi(GPIOC, 15);
    PinSetupOut(GPIOC, 14, omPushPull);
    Uart.Init();
    ReadParams();

    if(!Sleep::WasInStandby()) {
        Led.Init();
        Led.StartOrRestart(lsqStart0);
        Printf("\r%S %S\rID=%u; Thr=%d; Pwr=%u\r", APP_NAME, XSTRINGIFY(BUILD_TIME), ID, RssiThr, PwrLvlId);
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
        if(!Sleep::WasInStandby()) { // Show CC is ok
            Led.StartOrRestart(lsqStart);
            chThdSleepMilliseconds(126);
            Iwdg::Reload();
            chThdSleepMilliseconds(126);
            Iwdg::Reload();
        }
        // Common CC params
        CC.SetPktSize(RPKT_LEN);
        CC.SetChannel(0);

        Iwdg::Reload();
        MeshTask();
        Iwdg::Reload();
        BeaconTask();
        Iwdg::Reload();
    }
    else { // CC failure
        Led.Init();
        Led.StartOrRestart(lsqFailure);
        for(int i=0; i<11; i++) {
            Iwdg::Reload();
            chThdSleepMilliseconds(126);
        }
    }

    // Enter sleep
    CC.EnterPwrDown();
    chSysLock();
    Iwdg::Reload();
    Sleep::EnterStandby();
    chSysUnlock();

    while(true); // Will never be here
}

void MeshTask() {
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
                SetPwr(Pkt.Value);
            }
        }
        CC.SetTxPower(PwrTable[MESH_PWR_LVL_ID]);

    }
}

void BeaconTask() {
    CC.SetTxPower(PwrTable[PwrLvlId]);
    PktTx.From = ID;
    PktTx.Cmd = rcmdBeacon;
    PktTx.PktID = PKTID_DO_NOT_RETRANSMIT;
    PktTx.Beacon.RssiThr = RssiThr;
    PktTx.Beacon.Damage = Damage;
    PinSetHi(GPIOC, 14); // DEBUG
    CC.Recalibrate();
    CC.Transmit(&PktTx, RPKT_LEN);
    PinSetLo(GPIOC, 14); // DEBUG
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

    else if(PCmd->NameIs("SetPwr")) {
        uint32_t NewPwr;
        if(PCmd->GetNext<uint32_t>(&NewPwr) != retvOk) { Ack(retvCmdError); return; }
        if(NewPwr > 11) NewPwr = 11;
        Ack(SetPwr(NewPwr));
    }

    else if(PCmd->NameIs("Set")) {
        int32_t NewID;
        if(PCmd->GetNext<int32_t>(&NewID) != retvOk) { Ack(retvCmdError); return; }
        int32_t NewThr;
        if(PCmd->GetNext<int32_t>(&NewThr) != retvOk) { Ack(retvCmdError); return; }
        uint32_t NewPwr;
        if(PCmd->GetNext<uint32_t>(&NewPwr) != retvOk) { Ack(retvCmdError); return; }
        if(NewPwr > 11) NewPwr = 11;
        Printf("Ack %u %u %u\r\n", SetID(NewID), SetThr(NewThr), SetPwr(NewPwr));
    }
}
#endif

#if 1 // ============================ Save/Load Params =========================
void ReadParams() {
    ID = EE::Read32(EE_ADDR_ID);
    RssiThr = EE::Read32(EE_ADDR_THRESHOLD);
    PwrLvlId = EE::Read32(EE_ADDR_PWR_LVL_ID);
    if(PwrLvlId > 11) PwrLvlId = 11;
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

uint8_t SetPwr(uint32_t NewPwrId) {
    uint8_t rslt = EE::Write32(EE_ADDR_PWR_LVL_ID, NewPwrId);
    if(rslt == retvOk) {
        PwrLvlId = NewPwrId;
        return retvOk;
    }
    else {
        Printf("EE error: %u\r", rslt);
        return retvFail;
    }
}
#endif
