/*
 * diagnostic.h
 *
 *  Created on: May 28, 2021
 *      Author: kasetkin
 */

#ifndef DIAGNOSTIC_H_
#define DIAGNOSTIC_H_


constexpr uint32_t DIAGNOSTIC_FLASH_DURATION = 500;
constexpr uint32_t RADIO_ANSWER_TIMEOUT = 1000;
constexpr uint32_t RADIO_CMD_RECEIVED_DURATION = 100;

constexpr uint16_t DIAGNOSTIC_CLIENT_ID = 2000;
constexpr uint16_t DIAGNOSTIC_AUTOSERVER_ID = 2001;

enum class DiagnosticCommand : uint16_t {
	error = 0,
	none = 1,
	requestFromPlayer = 3000,
	answerFromPlayer = 3001,
	requestFromDiagServer = 3002,
	answerFromDiagServer = 3003
};



#endif /* DIAGNOSTIC_H_ */
