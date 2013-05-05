/*
 * Copyright (c) 2013 by Felix Rusu <felix@lowpowerlab.com>
 * Library for facilitating wireless programming using an RFM12B radio (get library at: https://github.com/LowPowerLab/RFM12B)
 * and the SPI Flash memory library for arduino/moteino (get library at: TODO).
 * DEPENDS ON the two libraries mentioned above
 * Install all three of these libraries in your Arduino/libraries folder ([Arduino > Preferences] for location of Arduino folder)
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

#include <WirelessHEX.h>
#include <avr/wdt.h>

/// Checks whether the last message received was a wireless programming request handshake
/// If so it will start the handshake protocol, receive the new HEX image and 
/// store it on the external flash chip, then reboot
/// Assumes radio has been initialized and has just received a message (is not in SLEEP mode, and you called CRCPass())
/// Assumes flash is an external SPI flash memory chip that has been initialized
void CheckForWirelessHEX(RFM12B radio, SPIFlash flash, boolean DEBUG)
{
  //special FLASH command, enter a FLASH image exchange sequence
  if (*radio.DataLen >= 4 && radio.Data[0]=='F' && radio.Data[1]=='L' && radio.Data[2]=='X' && radio.Data[3]=='?')
  {
    byte remoteID = radio.GetSender();
    if (*radio.DataLen == 7 && radio.Data[4]=='E' && radio.Data[5]=='O' && radio.Data[6]=='F')
    { //sender must have not received EOF ACK so just resend
      radio.Send(remoteID, "FLX?OK",6);
    }
    else if (HandleWirelessHEXData(radio, remoteID, flash, DEBUG))
    {
      if (DEBUG) Serial.print("FLASH IMG TRANSMISSION SUCCESS!\n");
      resetUsingWatchdog(DEBUG);
    }
    else
    {
      if (DEBUG) Serial.print("Timeout, erasing written data ... ");
      flash.blockErase32K(0); //clear any written data
      if (DEBUG) Serial.println("DONE");
    }
  }
}

boolean HandleWirelessHEXData(RFM12B radio, byte remoteID, SPIFlash flash, boolean DEBUG) {
  uint8_t c;
  long now=0;
  uint16_t tmp,seq=0,len;
  char buffer[16];
  int timeout = 3000; //3s for flash data
  uint16_t bytesFlashed=10;

  radio.SendACK("FLX?OK",6); //ACK the HANDSHAKE
  if (DEBUG) Serial.println("FLX?OK (ACK sent)");

  //first clear the fist 32k block (dedicated to a new FLASH image)
  flash.blockErase32K(0);
  flash.writeBytes(0,"FLXIMG:", 7);
  flash.writeByte(9,':');
  now=millis();
    
  while(1)
  {
    if (radio.ReceiveComplete() && radio.CRCPass() && radio.GetSender() == remoteID)
    {
      byte dataLen = *radio.DataLen;

      if (dataLen >= 4 && radio.Data[0]=='F' && radio.Data[1]=='L' && radio.Data[2]=='X')
      {
        if (radio.Data[3]==':' && dataLen >= 7) //FLX:_:_
        {
          byte index=3;
          tmp = 0;
          
          //read packet SEQ
          for (byte i = 4; i<8; i++) //up to 4 characters for seq number
          {
            index++;
            if (radio.Data[i] >=48 && radio.Data[i]<=57)
              tmp = tmp*10+radio.Data[i]-48;
            else if (radio.Data[i]==':')
            {
              if (i==4)
                return false;
              else break;
            }
          }

          if (DEBUG) {
            Serial.print("radio [");
            Serial.print(dataLen);
            Serial.print("] > ");
            PrintHex83((byte*)radio.Data, dataLen);
          }

          if (radio.Data[index++] != ':') return false;
          now = millis(); //got "good" packet

          if (tmp==seq)
          {
            for(byte i=index;i<dataLen;i++)
              flash.writeByte(bytesFlashed++, radio.Data[i]);

            //send ACK
            tmp = sprintf(buffer, "FLX:%u:OK", tmp);
            if (DEBUG) Serial.println((char*)buffer);
            radio.SendACK(buffer, tmp);
            seq++;
          }
        }

        if (radio.Data[3]=='?')
        {
          if (dataLen==4) //ACK for handshake was lost, resend
          {
            radio.SendACK("FLX?OK",6);
            if (DEBUG) Serial.println("FLX?OK");
          }
          if (dataLen==7 && radio.Data[4]=='E' && radio.Data[5]=='O' && radio.Data[6]=='F') //Expected EOF
          {
            if ((bytesFlashed-10)>31744) {
              if (DEBUG) Serial.println("IMG exceeds 31k");
              
              return false; //just return, let MAIN timeout
            }
            radio.SendACK("FLX?OK",6);
            if (DEBUG) Serial.println("FLX?OK");

            //save # of bytes written
            flash.writeByte(7,(bytesFlashed-10)>>8);
            flash.writeByte(8,(bytesFlashed-10));
            flash.writeByte(9,':');
            return true;
          }
        }
      }
      #ifdef LED //blink!
      pinMode(LED,OUTPUT); digitalWrite(LED,HIGH); delay(1); digitalWrite(LED,LOW);
      #endif
    }
    
    //abort FLASH sequence if no valid packet received for a long time
    if (millis()-now > timeout)
    {
      return false;
    }
  }
}

// reads a line feed (\n) terminated line from the serial stream
// returns # of bytes read, up to 255
// timeout in ms, will timeout and return after so long
byte readSerialLine(char* input, char endOfLineChar, byte maxLength, uint16_t timeout)
{
  byte inputLen = 0;
  Serial.setTimeout(timeout);
  inputLen = Serial.readBytesUntil(endOfLineChar, input, maxLength);
  input[inputLen]=0;//null-terminate it
  Serial.setTimeout(0);
  //Serial.println();
  return inputLen;
}

/// returns TRUE if a HEX file transmission was detected and it was actually transmitted successfully
boolean CheckForSerialHEX(byte* input, byte inputLen, RFM12B radio, byte targetID, uint16_t TIMEOUT, uint16_t ACKTIMEOUT, boolean DEBUG)
{
  if (inputLen == 4 && input[0]=='F' && input[1]=='L' && input[2]=='X' && input[3]=='?') {
    if (HandleSerialHandshake(radio, targetID, false, TIMEOUT, ACKTIMEOUT, DEBUG))
    {
      Serial.println("FLX?OK"); //signal serial handshake back to host script
      if (HandleSerialHEXData(radio, targetID, TIMEOUT, ACKTIMEOUT, DEBUG))
      {
        Serial.println("FLX?OK"); //signal EOF serial handshake back to host script
        if (DEBUG) Serial.println("FLASH IMG TRANSMISSION SUCCESS");
        return true;
      }
      if (DEBUG) Serial.println("FLASH IMG TRANSMISSION FAIL");
      return false;
    }
  }
  return false;
}

boolean HandleSerialHandshake(RFM12B radio, byte targetID, boolean isEOF, uint16_t TIMEOUT, uint16_t ACKTIMEOUT, boolean DEBUG)
{
  //temp
  //return true;

  byte retries = 3;
  long now = millis();
  
  while (millis()-now<TIMEOUT)
  {
    radio.Send(targetID, isEOF ? "FLX?EOF" : "FLX?", isEOF?7:4, true);
    if (waitForAck(radio, ACKTIMEOUT))
    {
      if (*radio.DataLen == 6 && radio.Data[0]=='F' && radio.Data[1]=='L' && radio.Data[2]=='X' && 
          radio.Data[3]=='?' && radio.Data[4]=='O' && radio.Data[5]=='K')
        return true;
    }
  }

  if (DEBUG) Serial.println("Handshake fail");
  return false;
}

boolean HandleSerialHEXData(RFM12B radio, byte targetID, uint16_t TIMEOUT, uint16_t ACKTIMEOUT, boolean DEBUG) {
  long now=millis();
  uint16_t seq=0, tmp=0, inputLen;
  byte remoteID = radio.GetSender(); //save the remoteID as soon as possible
  byte sendBuf[32];
  char input[64];
  //a FLASH record should not be more than 64 bytes: FLX:9999:10042000FF4FA591B4912FB7F894662321F48C91D6 

  while(1) {
    inputLen = readSerialLine(input);
    if (inputLen == 0) goto timeoutcheck;
    tmp = 0;
    
    if (inputLen >= 6) { //FLX:9:
      if (input[0]=='F' && input[1]=='L' && input[2]=='X')
      {
        if (input[3]==':')
        {
          byte index = 3;
          for (byte i = 4; i<8; i++) //up to 4 characters for seq number
          {
            index++;
            if (input[i] >=48 && input[i]<=57)
              tmp = tmp*10+input[i]-48;
            else if (input[i]==':')
            {
              if (i==4)
                return false;
              else break;
            }
          }
          //Serial.print("input[index] = ");Serial.print("[");Serial.print(index);Serial.print("]=");Serial.println(input[index]);
          if (input[index++] != ':') return false;
          now = millis(); //got good packet
          
          byte hexDataLen = validateHEXData(input+index, inputLen-index);
          if (hexDataLen>0)
          {
            if (tmp==seq) //only read data when packet number is the next expected SEQ number
            {
              byte sendBufLen = prepareSendBuffer(input+index+8, sendBuf, hexDataLen, seq); //extract HEX data from input to BYTE data into sendBuf (go from 2 HEX bytes to 1 byte), +8 will jump over the header directly to the HEX raw data
              //Serial.print("PREP ");Serial.print(sendBufLen); Serial.print(" > "); PrintHex83(sendBuf, sendBufLen);
              
              //SEND RADIO DATA
              if (sendHEXPacket(radio, remoteID, sendBuf, sendBufLen, seq, TIMEOUT, ACKTIMEOUT, DEBUG))
              {
                sprintf((char*)sendBuf, "FLX:%u:OK",seq);
                Serial.println((char*)sendBuf); //response to host (python?)
                seq++;
              }
              else return false;
            }
          }
          else Serial.println("FLX:INV");
        }
        if (inputLen==7 && input[3]=='?' && input[4]=='E' && input[5]=='O' && input[6]=='F')
        {
          //SEND RADIO EOF
          return HandleSerialHandshake(radio, targetID, true, TIMEOUT, ACKTIMEOUT, DEBUG);
        }
      }
    }
    
    //abort FLASH sequence if no valid packet received for a long time
timeoutcheck:
    if (millis()-now > TIMEOUT)
    {
      Serial.print("Timeout receiving FLASH image from SERIAL, aborting...");
      //send abort msg or just let node timeout as well?
      return false;
    }
  }
  return true;
}

//returns length of HEX data bytes if everything is valid
//returns 0 if any validation failed
byte validateHEXData(void* data, byte length)
{
  //assuming 1 byte record length, 2 bytes address, 1 byte record type, N data bytes, 1 CRC byte
  char* input = (char*)data;
  if (length <12 || length%2!=0) return 0; //shortest possible intel data HEX record is 12 bytes
  //Serial.print("VAL > "); Serial.println((char*)input);

  uint16_t checksum=0;
  //check valid HEX data and CRC
  for (byte i=0; i<length;i++)
  {
    if (!((input[i] >=48 && input[i]<=57) || (input[i] >=65 && input[i]<=70))) //0-9,A-F
      return 0;
    if (i%2 && i<length-2) checksum+=BYTEfromHEX(input[i-1], input[i]);
  }
  checksum=(checksum^0xFFFF)+1;
  
  //TODO : CHECK for address continuity (intel HEX addresses are big endian)
  
  //Serial.print("final CRC:");Serial.println((byte)checksum, HEX);
  //Serial.print("CRC byte:");Serial.println(BYTEfromHEX(input[length-2], input[length-1]), HEX);

  //check CHECKSUM byte
  if (((byte)checksum) != BYTEfromHEX(input[length-2], input[length-1]))
    return false;

  byte dataLength = BYTEfromHEX(input[0], input[1]); //length of actual HEX flash data (usually 16bytes)
  //calculate record length
  if (length != dataLength*2 + 10) //add headers and checksum bytes (a total of 10 combined)
    return 0;

  return dataLength; //all validation OK!
}

//returns the final size of the buf
byte prepareSendBuffer(char* hexdata, byte*buf, byte length, uint16_t seq)
{
  byte seqLen = sprintf(((char*)buf), "FLX:%u:", seq);
  for (byte i=0; i<length;i++)
    buf[seqLen+i] = BYTEfromHEX(hexdata[i*2], hexdata[i*2+1]);
  return seqLen+length;
}

//assume A and B are valid HEX chars [0-9A-F]
byte BYTEfromHEX(char MSB, char LSB)
{
  return (MSB>=65?MSB-55:MSB-48)*16 + (LSB>=65?LSB-55:LSB-48);
}

//return the SEQ of the ACK received, or -1 if invalid
boolean sendHEXPacket(RFM12B radio, byte targetID, byte* sendBuf, byte hexDataLen, uint16_t seq, uint16_t TIMEOUT, uint16_t ACKTIMEOUT, boolean DEBUG)
{
  long now = millis();
  
  while(1) {
    if (DEBUG) { Serial.print("RFTX > "); PrintHex83(sendBuf, hexDataLen); }
    radio.Send(targetID, sendBuf, hexDataLen, true);
    
    if (waitForAck(radio, ACKTIMEOUT))
    {
      byte ackLen = *radio.DataLen;
      
      if (DEBUG) { Serial.print("RFACK > "); Serial.print(ackLen); Serial.print(" > "); PrintHex83((byte*)radio.Data, ackLen); }
      
      if (ackLen >= 8 && radio.Data[0]=='F' && radio.Data[1]=='L' && radio.Data[2]=='X' && 
          radio.Data[3]==':' && radio.Data[ackLen-3]==':' &&
          radio.Data[ackLen-2]=='O' && radio.Data[ackLen-1]=='K')
      {
        uint16_t tmp=0;
        sscanf((const char*)radio.Data, "FLX:%u:OK", &tmp);
        return tmp == seq;
      }
    }

    if (millis()-now > TIMEOUT)
    {
      Serial.println("Timeout waiting for packet ACK, aborting FLASH operation ...");
      break; //abort FLASH sequence if no valid ACK was received for a long time
    }
  }
  return false;
}

// wait a few milliseconds for proper ACK to me, return true if indeed received
boolean waitForAck(RFM12B radio, uint16_t ACKTIMEOUT)
{
  long now = millis();
  while (millis() - now <= ACKTIMEOUT) {
    if (radio.ACKReceived(radio.GetSender()))
      return true;
  }
  return false;
}

void PrintHex83(uint8_t *data, uint8_t length) // prints 8-bit data in hex
{
  char tmp[length*2+1];
  byte first ;
  int j=0;
  for (uint8_t i=0; i<length; i++) 
  {
    first = (data[i] >> 4) | 48;
    if (first > 57) tmp[j] = first + (byte)39;
    else tmp[j] = first ;
    j++;

    first = (data[i] & 0x0F) | 48;
    if (first > 57) tmp[j] = first + (byte)39; 
    else tmp[j] = first;
    j++;
  }
  tmp[length*2] = 0;
  Serial.println(tmp);
}

/// Use watchdog to reset
void resetUsingWatchdog(boolean DEBUG)
{
  //wdt_disable();
  if (DEBUG) Serial.print("REBOOTING");
  wdt_enable(WDTO_15MS);
  while(1) if (DEBUG) Serial.print('.');
}
