/*
 * File:   main.cpp
 * Author: Kreyl
 * Project: Armlet2South
 *
 * Created on Feb 05, 2013, 20:27
 */

#include "kl_lib.h"
#include "ch.h"
#include "hal.h"
#include "uart.h"
#include "radio_lvl1.h"
//#include "vibro.h"
#include "led.h"
#include "Sequences.h"

#if 1 // ======================== Variables and defines ========================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
extern CmdUart_t Uart;
static void ITask();
static void OnCmd(Shell_t *PShell);

#define ID_MIN      82
#define ID_MAX      (RSLOT_CNT - 1)
#define ID_DEFAULT  82

// EEAddresses
#define EE_ADDR_DEVICE_ID       0
#define EE_ADDR_INFLUENCE       8

uint16_t ID;
uint8_t Influence;
static uint8_t ISetID(int32_t NewID);
static void ReadIDfromEE();
static uint8_t ISetInfluence(uint8_t NewInf);
static void ReadInfFromEE();

LedRGB_t Led { LED_R_PIN, LED_G_PIN, LED_B_PIN };

TmrKL_t TmrEverySecond {MS2ST(999), evtIdEverySecond, tktPeriodic};
uint32_t ArmletAppearTimeout = 0;
#endif

int main(void) {
    // ==== Init Vcore & clock system ====
    SetupVCore(vcore1V5);
    Clk.SetMSI4MHz();
    Clk.UpdateFreqValues();
    // ==== Init OS ====
    halInit();
    chSysInit();
    EvtQMain.Init();

    // ==== Init Hard & Soft ====
    Uart.Init(115200);
    ReadIDfromEE();
    ReadInfFromEE();
    Printf("\r%S %S\rID=%u; Influence=%u\r", APP_NAME, BUILD_TIME, ID, Influence);
    Clk.PrintFreqs();

    Led.Init();
    TmrEverySecond.StartOrRestart();

    if(Radio.Init() == retvOk) {
        if(ID == ID_FIREFLY) Led.StartOrRestart(lsqFirefly);
        else Led.StartOrRestart(lsqStart);
    }
    else Led.StartOrRestart(lsqFailure);

    // Main cycle
    ITask();
}

__noreturn
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
        switch(Msg.ID) {
            case evtIdEverySecond:
//                Printf("Second\r");
                if(ArmletAppearTimeout > 0) {
                    ArmletAppearTimeout--;
                    if(ArmletAppearTimeout == 0) Led.StartOrRestart(lsqDisappear);
                }
//                Printf("ArmletAppearTimeout: %u\r", ArmletAppearTimeout);
                break;

            case evtIdShellCmd:
                OnCmd((Shell_t*)Msg.Ptr);
                ((Shell_t*)Msg.Ptr)->SignalCmdProcessed();
                break;

            case evtIdNewRPkt: {
                Param_t *PPar;
                PPar = (Param_t*)&Msg.b[1];
                bool IsInTodash = PPar->IsInTodash;
                int8_t Rssi = (int8_t)Msg.b[2];
//                Printf("Rssi: %d; IsInTodash: %u\r", (int8_t)Msg.b[2], IsInTodash);
                if(IsInTodash and ID == ID_FIREFLY and Rssi > -75) {
                    if(ArmletAppearTimeout == 0) Led.StartOrRestart(lsqAppear);
                    ArmletAppearTimeout = 18;
                } // in Todash
            } break;

            default: Printf("Unhandled Msg %u\r", Msg.ID); break;
        } // switch
    } // while true
}

void OnCmd(Shell_t *PShell) {
    Cmd_t *PCmd = &PShell->Cmd;
    __attribute__((unused)) int32_t dw32 = 0;  // May be unused in some configurations
//    Uart.Printf("%S\r", PCmd->Name);
    // Handle command
    if(PCmd->NameIs("Ping")) {
        PShell->Ack(retvOk);
    }
    else if(PCmd->NameIs("Version")) PShell->Printf("%S %S\r", APP_NAME, BUILD_TIME);

    else if(PCmd->NameIs("GetID")) PShell->Reply("ID", ID);
    else if(PCmd->NameIs("SetID")) {
        if(PCmd->GetNext<uint16_t>(&ID) != retvOk) { PShell->Ack(retvCmdError); return; }
        uint8_t r = ISetID(ID);
        PShell->Ack(r);
    }

    else if(PCmd->NameIs("GetInf")) PShell->Reply("Inf", Influence);
    else if(PCmd->NameIs("SetInf")) {
        uint8_t NewInf;
        if(PCmd->GetNext<uint8_t>(&NewInf) != retvOk) { PShell->Ack(retvCmdError); return; }
        uint8_t r = ISetInfluence(NewInf);
        PShell->Ack(r);
    }

    else PShell->Ack(retvCmdUnknown);
}

#if 1 // =========================== EE management =============================
void ReadIDfromEE() {
    ID = EE::Read32(EE_ADDR_DEVICE_ID);  // Read device ID
    if((ID < ID_MIN or ID > ID_MAX) and (ID != ID_FIREFLY)) {
        Printf("\rUsing default ID\r");
        ID = ID_DEFAULT;
    }
}

uint8_t ISetID(int32_t NewID) {
    if((NewID < ID_MIN or NewID > ID_MAX) and (ID != ID_FIREFLY)) return retvFail;
    uint8_t rslt = EE::Write32(EE_ADDR_DEVICE_ID, NewID);
    if(rslt == retvOk) {
        ID = NewID;
        Printf("New ID: %u\r", ID);
        return retvOk;
    }
    else {
        Printf("EE error: %u\r", rslt);
        return retvFail;
    }
}

void ReadInfFromEE() {
    Influence = EE::Read32(EE_ADDR_INFLUENCE);
}

uint8_t ISetInfluence(uint8_t NewInf) {
    uint8_t rslt = EE::Write32(EE_ADDR_INFLUENCE, NewInf);
    if(rslt == retvOk) {
        Influence = NewInf;
        Printf("New Inf: %u\r", Influence);
        return retvOk;
    }
    else {
        Printf("EE error: %u\r", rslt);
        return retvFail;
    }
}

#endif
