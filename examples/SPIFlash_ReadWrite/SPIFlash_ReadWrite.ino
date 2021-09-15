// **********************************************************************************
// This sketch is an example of using the SPIFlash library with a Moteino
// that has an onboard SPI Flash chip. This sketch listens to a few serial commands
// Hence type the following commands to interact with the SPI flash memory array:
//'c' - read flash chip's deviceID 10 times to ensure chip is present
//'d' - dump first 256 bytes on the chip
//'e' - erase entire flash chip
//'E' - erase last 4K block in flash chip
//'D' - dump first 256 bytes in the last 4K block
//'i' - read deviceID
//'0'-'9' - write 0xaa/0xbb bytes at addresses 0..9, 520192..520447
// Get the SPIFlash library from here: https://github.com/LowPowerLab/SPIFlash
// Note: if other SPI devices are present, ensure their CS pins are pulled up or set HIGH
// **********************************************************************************
// (C) 2021 Felix Rusu, LowPowerLab.com
// Library and code by Felix Rusu - felix@lowpowerlab.com
// **********************************************************************************
// License
// **********************************************************************************
// This program is free software; you can redistribute it 
// and/or modify it under the terms of the GNU General    
// Public License as published by the Free Software       
// Foundation; either version 3 of the License, or        
// (at your option) any later version.                    
//                                                        
// This program is distributed in the hope that it will   
// be useful, but WITHOUT ANY WARRANTY; without even the  
// implied warranty of MERCHANTABILITY or FITNESS FOR A   
// PARTICULAR PURPOSE. See the GNU General Public        
// License for more details.                              
//                                                        
// You should have received a copy of the GNU General    
// Public License along with this program.
// If not, see <http://www.gnu.org/licenses/>.
//                                                        
// Licence can be viewed at                               
// http://www.gnu.org/licenses/gpl-3.0.txt
//
// Please maintain this license information along with authorship
// and copyright notices in any redistribution of this code
// **********************************************************************************
#include <SPIFlash.h>    //get it here: https://github.com/LowPowerLab/SPIFlash

#define SERIAL_BAUD      115200
char input = 0;
long lastPeriod = -1;

//////////////////////////////////////////
// flash(SPI_CS, MANUFACTURER_ID)
// SPI_CS          - CS pin attached to SPI flash chip (8 in case of Moteino)
// MANUFACTURER_ID - OPTIONAL, 0x1F44 for adesto(ex atmel) 4mbit flash
//                             0xEF30 for windbond 4mbit flash
//                             0xEF40 for windbond 64mbit flash
//////////////////////////////////////////
uint16_t expectedDeviceID=0xEF30;
SPIFlash flash(SS_FLASHMEM, expectedDeviceID);

void setup(){
#ifdef SERIAL_BAUD
  Serial.begin(SERIAL_BAUD);
  while (!Serial) delay(100); //wait until Serial/monitor is opened
#endif
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.print("Start...");

  //ensure the radio module CS pin is pulled HIGH or it might interfere!
  pinMode(SS, OUTPUT); digitalWrite(SS, HIGH);

  if (flash.initialize())
  {
    Serial.println("Init OK!");
    Blink(20, 10);
  }
  else {
    Serial.print("Init FAIL, expectedDeviceID(0x");
    Serial.print(expectedDeviceID, HEX);
    Serial.print(") mismatched the read value: 0x");
    Serial.println(flash.readDeviceId(), HEX);
  }

  Serial.println("\n************************");
  Serial.println("Available operations:");
  Serial.println("'c' - read flash chip's deviceID 10 times to ensure chip is present");
  Serial.println("'d' - dump first 256 bytes on the chip");
  Serial.println("'e' - erase entire flash chip");
  Serial.println("'E' - erase last 4K block in flash chip");
  Serial.println("'D' - dump first 256 bytes in the last 4K block");
  Serial.println("'i' - read deviceID");
  Serial.println("'0'-'9' - write 0xaa/0xbb bytes at addresses 0..9, 520192..520447");
  Serial.println("************************\n");
  delay(1000);
}

void loop(){
  // Handle serial input (to allow basic DEBUGGING of FLASH chip)
  // ie: display first 256 bytes in FLASH, erase chip, write bytes at first 10 positions, etc
  if (Serial.available() > 0) {
    input = Serial.read();
    if (input == 'd') { //d=dump flash area
      Serial.println("Flash content (256 bytes, starting address 0):");
      int counter = 0;

      while(counter<256){
        Serial.print(flash.readByte(counter), HEX);
        counter++;
        if (counter%16 == 0) Serial.println(); else Serial.print('.');
      }
      Serial.println();
    }
    else if (input == 'D') { //d=dump flash area @ last 4K block
      Serial.println("Flash content (256bytes, starting address 520192):");
      uint32_t counter = 520192;

      while(counter<520192+256){
        Serial.print(flash.readByte(counter), HEX);
        counter++;
        if (counter%16 == 0) Serial.println(); else Serial.print('.');
      }
      Serial.println();
    }
    else if (input == 'c') {
      Serial.print("Checking chip is present ... ");
      uint16_t deviceID=0;
      for (uint8_t i=0;i<10;i++) {
        uint16_t idNow = flash.readDeviceId();
        if (idNow==0 || idNow==0xffff || (i>0 && idNow != deviceID)) {
          deviceID=0;
          break;
        }
        deviceID=idNow;
      }
      if (deviceID!=0) {
        Serial.print("OK, deviceID=0x");Serial.println(deviceID, HEX);
      }
      else Serial.println("FAIL, deviceID is inconsistent or 0x0000/0xffff");
    }
    else if (input == 'e')
    {
      Serial.print("Erasing Flash chip ... ");
      flash.chipErase();
      while(flash.busy());
      Serial.println("DONE");
    }
    else if (input == 'E')
    {
      Serial.print("Erasing last 4K block @520192 ... ");
      flash.blockErase4K(520192);
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
      flash.writeByte(input-48+520192, millis()%2 ? 0xaa : 0xbb);
    }
  }

  // Periodically blink the onboard LED while listening for serial commands
  if ((int)(millis()/500) > lastPeriod) {
    lastPeriod++;
    digitalWrite(LED_BUILTIN, lastPeriod%2);
  }
}

void Blink(int DELAY_MS, byte loops)
{
  while (loops--)
  {
    digitalWrite(LED_BUILTIN,HIGH);
    delay(DELAY_MS);
    digitalWrite(LED_BUILTIN,LOW);
    delay(DELAY_MS);  
  }
}