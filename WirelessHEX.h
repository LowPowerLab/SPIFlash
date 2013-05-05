/*
 * Copyright (c) 2013 by Felix Rusu <felix@lowpowerlab.com>
 * Library for facilitating wireless programming using an RFM12B radio (get library at: http://github.com/LowPowerLab/RFM12B)
 * and the SPI Flash memory library for arduino/moteino (get library at: http://github.com/LowPowerLab/SPIFlash)
 * DEPENDS ON the two libraries mentioned above, and comes bundled with the SPIFlash library (above link)
 * Install all three of these libraries in your Arduino/libraries folder ([Arduino > Preferences] for location of Arduino folder)
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

#ifndef _WirelessHEX_H_
#define _WirelessHEX_H_
#define LED 9  //LED on digital pin 9

#ifndef DEFAULT_TIMEOUT
  #define DEFAULT_TIMEOUT 3000
#endif

#ifndef ACK_TIMEOUT
  #define ACK_TIMEOUT 50
#endif

#include <RFM12B.h>
#include <SPIFlash.h>

//functions used in the REMOTE node
void CheckForWirelessHEX(RFM12B radio, SPIFlash flash, boolean DEBUG=false);
void resetUsingWatchdog(boolean DEBUG=false);
boolean HandleWirelessHEXData(RFM12B radio, byte remoteID, SPIFlash flash, boolean DEBUG=false);

//functions used in the MAIN node
boolean CheckForSerialHEX(byte* input, byte inputLen, RFM12B radio, byte targetID, uint16_t TIMEOUT=DEFAULT_TIMEOUT, uint16_t ACKTIMEOUT=ACK_TIMEOUT, boolean DEBUG=false);
boolean HandleSerialHandshake(RFM12B radio, byte targetID, boolean isEOF, uint16_t TIMEOUT=DEFAULT_TIMEOUT, uint16_t ACKTIMEOUT=ACK_TIMEOUT, boolean DEBUG=false);
boolean HandleSerialHEXData(RFM12B radio, byte targetID, uint16_t TIMEOUT=DEFAULT_TIMEOUT, uint16_t ACKTIMEOUT=ACK_TIMEOUT, boolean DEBUG=false);
boolean waitForAck(RFM12B radio, uint16_t ACKTIMEOUT=ACK_TIMEOUT);

byte validateHEXData(void* data, byte length);
byte prepareSendBuffer(char* hexdata, byte*buf, byte length, uint16_t seq);
boolean sendHEXPacket(RFM12B radio, byte remoteID, byte* sendBuf, byte hexDataLen, byte seq, uint16_t ACKTIMEOUT=ACK_TIMEOUT, uint16_t TIMEOUT=DEFAULT_TIMEOUT, boolean DEBUG=false);
byte BYTEfromHEX(char MSB, char LSB);
byte readSerialLine(void* input);
void PrintHex83(byte* data, byte length);

#endif
