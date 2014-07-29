/*
 * Copyright (c) 2013 by Felix Rusu <felix@lowpowerlab.com>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

// This sketch is an example of using the SPIFlash library with a Moteino
// that has an onboard SPI Flash chip. This sketch listens to a few serial commands
// Hence type the following commands to interact with the SPI flash memory array:
// - 'd' dumps the first 256bytes of the flash chip to screen
// - 'e' erases the entire memory chip
// - 'i' print manufacturer/device ID
// - [0-9] writes a random byte to addresses [0-9] (either 0xAA or 0xBB)
// Get the SPIFlash library from here: https://github.com/LowPowerLab/SPIFlash

#include <SPIFlash.h>
#include <SPI.h>

#define SERIAL_BAUD      115200
char input = 0;
long lastPeriod = -1;

//////////////////////////////////////////
// flash(SPI_CS, MANUFACTURER_ID)
// SPI_CS          - CS pin attached to SPI flash chip (8 in case of Moteino)
// MANUFACTURER_ID - OPTIONAL, 0x1F44 for adesto(ex atmel) 4mbit flash
//                             0xEF30 for windbond 4mbit flash
//////////////////////////////////////////
SPIFlash flash(8, 0xEF30);

void setup(){
  Serial.begin(SERIAL_BAUD);
  Serial.print("Start...");

  if (flash.initialize())
    Serial.println("Init OK!");
  else
    Serial.println("Init FAIL!");
}

void loop(){
  // Handle serial input (to allow basic DEBUGGING of FLASH chip)
  // ie: display first 256 bytes in FLASH, erase chip, write bytes at first 10 positions, etc
  if (Serial.available() > 0) {
    input = Serial.read();
    if (input == 'd') //d=dump flash area
    {
      Serial.println("Flash content:");
      int counter = 0;

      while(counter<=256){
        Serial.print(flash.readByte(counter++), HEX);
        Serial.print('.');
      }
      
      Serial.println();
    }
    else if (input == 'e')
    {
      Serial.print("Erasing Flash chip ... ");
      flash.chipErase();
      while(flash.busy());
      Serial.println("DONE");
    }
    else if (input == 'i')
    {
      Serial.print("DeviceID: ");
      Serial.println(flash.readDeviceId(), HEX);
    }
    else if (input >= 48 && input <= 57) //0-9
    {
      Serial.print("\nWriteByte("); Serial.print(input); Serial.print(")");
      flash.writeByte(input-48, millis()%2 ? 0xaa : 0xbb);
    }
  }

  // Periodically blink the onboard LED while listening for serial commands
  if ((int)(millis()/500) > lastPeriod)
  {
    lastPeriod++;
    pinMode(9, OUTPUT);
    digitalWrite(9, lastPeriod%2);
  }
}
