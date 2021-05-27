/*
 * glue.h
 *
 *  Created on: 4 ��� 2019 �.
 *      Author: Kreyl
 */

#ifndef GLUE_H_
#define GLUE_H_

// Saving character params (loading is done at power-up)
void SaveState(uint32_t AState);
void SaveHP(uint32_t HP);
void SaveTimerAgony(uint32_t ATimer);

// Indication
void BeepForPeriod(uint32_t APeriod_ms);
void Flash(uint8_t R, uint8_t G, uint8_t B, uint32_t Duration_ms);
void SetDefaultColor(uint8_t R, uint8_t G, uint8_t B);

// Pill
void ClearPill();

// Events


#endif /* GLUE_H_ */
