#include "board.h"
#include "led.h"
#include "vibro.h"
#include "beeper.h"
#include "Sequences.h"
#include "glue.h"
#include "diagnostic.h"
#include "radio_lvl1.h"
#include "kl_i2c.h"
#include "kl_lib.h"
#include "pill.h"
#include "pill_mgr.h"
#include "MsgQ.h"
#include "main.h"

#define STATE_MACHINE_EN    TRUE

#if STATE_MACHINE_EN
#include "qhsm.h"
#include "eventHandlers.h"
#include "oregonPill.h"
#endif

#if 1 // ======================== Variables and defines ========================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
static void ITask();
static void OnCmd(Shell_t *PShell);

// EEAddresses
#define EE_ADDR_DEVICE_ID   0
// StateMachines
#define EE_ADDR_STATE       2048
#define EE_ADDR_HP          2052
#define EE_ADDR_AGONY       2056

#if STATE_MACHINE_EN
void InitSM();
void SendEventSMPill(int QSig, unsigned int SrcID, unsigned int Value);
void SendEventSMPlayer(int QSig, unsigned int SrcID, unsigned int Value);
static oregonPlayerQEvt ePlayer;
static oregonPillQEvt ePill;
#endif

int32_t ID;
static uint8_t ISetID(int32_t NewID);
void ReadIDfromEE();

// ==== Periphery ====
#if VIBRO_ENABLED
Vibro_t VibroMotor {VIBRO_SETUP};
#endif
#if BEEPER_ENABLED
Beeper_t Beeper {BEEPER_PIN};
#endif

LedRGB_t Led { LED_R_PIN, LED_G_PIN, LED_B_PIN };

// ==== Timers ====
static TmrKL_t TmrEverySecond {TIME_MS2I(1000), evtIdEverySecond, tktPeriodic};
//static TmrKL_t TmrRxTableCheck {MS2ST(2007), evtIdCheckRxTable, tktPeriodic};
uint32_t TimeS = 0;
#endif

void runDiagnostic();

int main(void) {
    // ==== Init Vcore & clock system ====
    SetupVCore(vcore1V5);
    Clk.SetMSI4MHz();
    Clk.UpdateFreqValues();

    // === Init OS ===
    halInit();
    chSysInit();
    EvtQMain.Init();

    // ==== Init hardware ====
    PinSetupOut(GPIOC, 15, omPushPull); // To measure total ON time
    PinSetHi(GPIOC, 15);
    PinSetupOut(GPIOC, 14, omPushPull); // To measure TX time

    Uart.Init();
    ReadIDfromEE();
    Printf("\r%S %S; ID=%u\r", APP_NAME, XSTRINGIFY(BUILD_TIME), ID);
    Clk.PrintFreqs();

    Led.Init();
#if VIBRO_ENABLED
    VibroMotor.Init();
#endif
#if BEEPER_ENABLED // === Beeper ===
    Beeper.Init();
    Beeper.StartOrRestart(bsqBeepBeep);
#endif
#if BUTTONS_ENABLED
    SimpleSensors::Init();
#endif

#if PILL_ENABLED // === Pill ===
    i2c1.Init();
    PillMgr.Init();
#endif

    // ==== Time and timers ====
    TmrEverySecond.StartOrRestart();

    // ==== Radio ====
    if(Radio.Init() == retvOk)
    	Led.StartOrRestart(lsqStart);
    else
    	Led.StartOrRestart(lsqFailure);

#if VIBRO_ENABLED
    VibroMotor.StartOrRestart(vsqBrrBrr);
#endif
    chThdSleepMilliseconds(1008);

#if STATE_MACHINE_EN
    InitSM();
#endif

    // Main cycle
    ITask();
}

__noreturn
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
        switch(Msg.ID) {
            case evtIdEverySecond:
                PillMgr.Check();
                TimeS++;
#if STATE_MACHINE_EN
                SendEventSMPlayer(TIME_TICK_1S_SIG, 0, 0);
                SendEventSMPill(TIME_TICK_1S_SIG, 0, 0);
                if((TimeS % 10) == 0) {
                    SendEventSMPlayer(TIME_TICK_10S_SIG, 0, 0);
                }
                if((TimeS % 60) == 0) {
                    SendEventSMPlayer(TIME_TICK_1M_SIG, 0, 0);
                    SendEventSMPill(TIME_TICK_1M_SIG, 0, 0);
                }
#endif
                // Check RX data
                for(int32_t i=0; i<LUSTRA_CNT; i++) {
                    if(Radio.RxData[i].ProcessAndCheck()) {
#if STATE_MACHINE_EN
                        SendEventSMPlayer(RAD_RCVD_SIG, (i+LUSTRA_MIN_ID), Radio.RxData[i].Dmg);
#endif
                    }
                }
                break;

#if BUTTONS_ENABLED
            case evtIdButtons:
//                Printf("Btn %u\r", Msg.BtnEvtInfo.BtnID);
                if(Msg.BtnEvtInfo.Type == beShortPress) {
                    SendEventSM(BUTTON_PRESSED_SIG, 0, 0);
                }
                else if(Msg.BtnEvtInfo.Type == beLongCombo and Msg.BtnEvtInfo.BtnCnt == 3) {
                    Printf("Combo\r");
                    SendEventSM(BUTTON_LONG_PRESSED_SIG, 0, 0);
                }
                break;
#endif

#if PILL_ENABLED // ==== Pill ====
            case evtIdPillConnected:
                Printf("Pill: %u\r", PillMgr.Pill.DWord32);
                {
                	uint32_t PillId = PillMgr.Pill.DWord32;
#if STATE_MACHINE_EN
					SendEventSMPill(PILL_ANY_SIG, 0, 0);
					switch(PillId) {
						case 0: SendEventSMPill(PILL_RESET_SIG, 0, 0); break;
						case 1: SendEventSMPill(PILL_ANTIRAD_SIG, 0, 0); break;
						case 2: SendEventSMPill(PILL_RAD_X_SIG, 0, 0); break;
						case 3: SendEventSMPill(PILL_HEAL_SIG, 0, 0); break;
						case 4: SendEventSMPill(PILL_HEALSTATION_SIG, 0, 0); break;
						case 5: SendEventSMPill(PILL_BLESS_SIG, 0, 0); break;
						case 6: SendEventSMPill(PILL_CURSE_SIG, 0, 0); break;
						case 7: SendEventSMPill(PILL_GHOUL_SIG, 0, 0); break;
						case 8: SendEventSMPill(PILL_TEST_SIG, 0, 0); break;
						default: break;
					}

#endif
                if (PillId == PILL_DIAGNOSTIC)
                	runDiagnostic();
                }
                break;

            case evtIdPillDisconnected:
                Printf("Pill disconn\r");
#if STATE_MACHINE_EN
                {
                	uint32_t PillId = PillMgr.Pill.DWord32;
					if (PillId == 7)
						SendEventSMPill(PILL_GHOUL_REMOVED_SIG, 0, 0);
					else
						SendEventSMPill(PILL_REMOVED_SIG, 0, 0);
                }
#endif
                break;
#endif

            case evtIdShellCmd:
                OnCmd((Shell_t*)Msg.Ptr);
                ((Shell_t*)Msg.Ptr)->SignalCmdProcessed();
                break;
            default: Printf("Unhandled Msg %u\r", Msg.ID); break;
        } // Switch
    } // while true
} // ITask()

#if 1 // ==== Glue functions ====
void SaveState(uint32_t AState) {
    if(EE::Write32(EE_ADDR_STATE, AState) != retvOk) Printf("Saving State fail\r");
}

void SaveHP(uint32_t HP) {
    if(EE::Write32(EE_ADDR_HP, HP) != retvOk) Printf("Saving HP fail\r");
}

void SaveTimerAgony(uint32_t ATimer) {
    if(EE::Write32(EE_ADDR_AGONY, ATimer) != retvOk) Printf("Saving Agony fail\r");
}

#if VIBRO_ENABLED
BaseChunk_t vsqSMBrr[] = {
        {csSetup, VIBRO_VOLUME},
        {csWait, 99},
        {csSetup, 0},
        {csEnd}
};

void Vibro(uint32_t Duration_ms) {
    vsqSMBrr[1].Time_ms = Duration_ms;
    VibroMotor.StartOrRestart(vsqSMBrr);
}
#endif

#if BEEPER_ENABLED
BeepChunk_t bsqSMBeep[] = {
        {csSetup, 7, 1975},
        {csWait, 54},
        {csSetup, 0},
        {csEnd}
};

void BeepForPeriod(uint32_t APeriod_ms) {
    bsqSMBeep[1].Time_ms = APeriod_ms;
    Beeper.StartOrRestart(bsqSMBeep);
}
#endif

// ==== LED ====
LedRGBChunk_t lsqSM[] = {
        {csSetup, 0, clRed},
        {csWait, 207},
        {csSetup, 0, {0,1,0}},
        {csEnd},
};

void Flash(uint8_t R, uint8_t G, uint8_t B, uint32_t Duration_ms) {
    lsqSM[0].Color.FromRGB(R, G, B);
    lsqSM[1].Time_ms = Duration_ms;
    Led.StartOrRestart(lsqSM);
}

void SetDefaultColor(uint8_t R, uint8_t G, uint8_t B) {
    lsqSM[2].Color.FromRGB(R, G, B);
    Led.StartOrRestart(lsqSM);
}


#define THE_WORD    0xCA115EA1
void ClearPill() {
    uint32_t DWord32 = THE_WORD;
    if(PillMgr.Write(0, &DWord32, 4) != retvOk) Printf("ClearPill fail\r");
}
#endif

#if STATE_MACHINE_EN // ======================== State Machines ===============================
void InitSM() {
    // Load saved data
    uint32_t HP = EE::Read32(EE_ADDR_HP);
    uint32_t State = EE::Read32(EE_ADDR_STATE);
    uint32_t TmrAgony = EE::Read32(EE_ADDR_AGONY);
//    Printf("Saved: HP=%d MaxHP=%d DefaultHP=%d State=%d\r", HP, MaxHP, DefaultHP, State);
    // Check if params are bad
    OregonPlayer_ctor(HP, State, TmrAgony);
    QMSM_INIT(the_oregonPlayer, (QEvt *)0);
    OregonPill_ctor();
    QMSM_INIT(the_oregonPill, (QEvt *)0);
}

void SendEventSMPlayer(int QSig, unsigned int SrcID, unsigned int Value) {
    ePlayer.super.sig = QSig;
    ePlayer.value = Value;
    Printf("ePlayer Sig: %d; value: %d\r", ePlayer.super.sig, ePlayer.value);
    QMSM_DISPATCH(the_oregonPlayer, &(ePlayer.super));
}

void SendEventSMPill(int QSig, unsigned int SrcID, unsigned int Value) {
    ePill.super.sig = QSig;
    Printf("ePill Sig: %d\r", ePill.super.sig);
    QMSM_DISPATCH(the_oregonPill, &(ePill.super));
}
#endif

#if 1 // ================= Command processing ====================
void OnCmd(Shell_t *PShell) {
	Cmd_t *PCmd = &PShell->Cmd;
    __attribute__((unused)) int32_t dw32 = 0;  // May be unused in some configurations
//    Uart.Printf("%S\r", PCmd->Name);
    // Handle command
    if(PCmd->NameIs("Ping")) {
        PShell->Ack(retvOk);
    }
    else if(PCmd->NameIs("Version")) PShell->Print("%S %S\r", APP_NAME, XSTRINGIFY(BUILD_TIME));

    else if(PCmd->NameIs("GetID")) PShell->Reply("ID", ID);

    else if(PCmd->NameIs("SetID")) {
        if(PCmd->GetNext<int32_t>(&ID) != retvOk) { PShell->Ack(retvCmdError); return; }
        uint8_t r = ISetID(ID);
        RMsg_t msg;
        msg.Cmd = R_MSG_SET_CHNL;
        msg.Value = ID2RCHNL(ID);
        Radio.RMsgQ.SendNowOrExit(msg);
        PShell->Ack(r);
    }

    else if(PCmd->NameIs("Rst")) {
        SendEventSMPill(PILL_RESET_SIG, 0, 0);
        PShell->Ack(retvOk);
    }

#if 1 // === Pill ===
    else if(PCmd->NameIs("ReadPill")) {
        int32_t DWord32;
        uint8_t Rslt = PillMgr.Read(0, &DWord32, 4);
        if(Rslt == retvOk) {
            PShell->Print("Read %d\r\n", DWord32);
        }
        else PShell->Ack(retvFail);
    }

    else if(PCmd->NameIs("WritePill")) {
        int32_t DWord32;
        if(PCmd->GetNext<int32_t>(&DWord32) == retvOk) {
            uint8_t Rslt = PillMgr.Write(0, &DWord32, 4);
            PShell->Ack(Rslt);
        }
        else PShell->Ack(retvCmdError);
    }
#endif

    else PShell->Ack(retvCmdUnknown);
}
#endif


#if 1 // =========================== ID management =============================
void ReadIDfromEE() {
    ID = EE::Read32(EE_ADDR_DEVICE_ID);  // Read device ID
    if(ID < ID_MIN or ID > ID_MAX) {
        Printf("\rUsing default ID\r");
        ID = ID_DEFAULT;
    }
}

uint8_t ISetID(int32_t NewID) {
    if(NewID < ID_MIN or NewID > ID_MAX) return retvFail;
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
#endif

void blockingFlash(uint8_t R, uint8_t G, uint8_t B, uint32_t Duration_ms)
{
	Flash(R, G, B, Duration_ms);
    chThdSleepMilliseconds(Duration_ms + 1);
}

void blockingBeep(uint32_t Duration_ms)
{
	BeepForPeriod(Duration_ms);
	chThdSleepMilliseconds(Duration_ms + 1);
}

void beepShort(size_t repeatCount = 1)
{
	for (size_t i = 0; i < repeatCount; ++i) {
		blockingBeep(50);
		chThdSleepMilliseconds(150);
	}
}

void beepLong(size_t repeatCount = 1)
{
	for (size_t i = 0; i < repeatCount; ++i) {
		blockingBeep(150);
		chThdSleepMilliseconds(50);
	}
}

void runDiagnostic()
{
	/// erase diagCmd, check it later, meanwhile flash LED and buzzer
	Radio.diagCmd = DiagnosticCommand::none;

	blockingFlash(255, 0, 0, DIAGNOSTIC_FLASH_DURATION);
	blockingFlash(0, 255, 0, DIAGNOSTIC_FLASH_DURATION);
	blockingFlash(0, 0, 255, DIAGNOSTIC_FLASH_DURATION);
	blockingBeep(DIAGNOSTIC_FLASH_DURATION);

	Radio.diagCmd = DiagnosticCommand::none;
	chThdSleepMilliseconds(RADIO_ANSWER_TIMEOUT);

	if (Radio.diagCmd == DiagnosticCommand::answerFromDiagServer) {
		blockingFlash(0, 255, 0, RADIO_CMD_RECEIVED_DURATION);
		blockingFlash(0, 4, 0, RADIO_CMD_RECEIVED_DURATION);
		blockingFlash(0, 255, 0, RADIO_CMD_RECEIVED_DURATION);
		blockingFlash(0, 4, 0, RADIO_CMD_RECEIVED_DURATION);
		blockingFlash(0, 255, 0, RADIO_CMD_RECEIVED_DURATION);
		blockingFlash(0, 4, 0, RADIO_CMD_RECEIVED_DURATION);
		beepLong(10);
	} else {
		blockingFlash(255, 0, 0, RADIO_CMD_RECEIVED_DURATION * 2);
		blockingFlash(0, 4, 0, RADIO_CMD_RECEIVED_DURATION);
		blockingFlash(255, 0, 0, RADIO_CMD_RECEIVED_DURATION * 2);
		blockingFlash(0, 4, 0, RADIO_CMD_RECEIVED_DURATION);
		blockingFlash(255, 0, 0, RADIO_CMD_RECEIVED_DURATION * 2);
		blockingFlash(0, 4, 0, RADIO_CMD_RECEIVED_DURATION);
		beepShort(3);
		beepLong(3);
		beepShort(3);
	}
}
