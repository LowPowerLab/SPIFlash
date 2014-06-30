/*
 * Copyright (c) 2013 by Felix Rusu <felix@lowpowerlab.com>
 * SPI Flash memory library for arduino/moteino.
 * This works with 256byte/page SPI flash memory
 * For instance a 4MBit (512Kbyte) flash chip will have 2048 pages: 256*2048 = 524288 bytes (512Kbytes)
 * Minimal modifications should allow chips that have different page size but modifications
 * DEPENDS ON: Arduino SPI library
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

#include <SPIFlash.h>

byte SPIFlash::UNIQUEID[8];

/// IMPORTANT: NAND FLASH memory requires erase before write, because
///            it can only transition from 1s to 0s and only the erase command can reset all 0s to 1s
/// See http://en.wikipedia.org/wiki/Flash_memory
/// The smallest range that can be erased is a sector (4K, 32K, 64K); there is also a chip erase command

/// Constructor. JedecID is optional but recommended, since this will ensure that the device is present and has a valid response
/// get this from the datasheet of your flash chip
/// Example for Atmel-Adesto 4Mbit AT25DF041A: 0x1F44 (page 27: http://www.adestotech.com/sites/default/files/datasheets/doc3668.pdf)
/// Example for Winbond 4Mbit W25X40CL: 0xEF30 (page 14: http://www.winbond.com/NR/rdonlyres/6E25084C-0BFE-4B25-903D-AE10221A0929/0/W25X40CL.pdf)
SPIFlash::SPIFlash(uint8_t slaveSelectPin, uint16_t jedecID) {
  _slaveSelectPin = slaveSelectPin;
  _jedecID = jedecID;
}

/// Select the flash chip
void SPIFlash::select() {
  noInterrupts();
  digitalWrite(_slaveSelectPin, LOW);
}

/// UNselect the flash chip
void SPIFlash::unselect(int us) {
  digitalWrite(_slaveSelectPin, HIGH);
  if (us > 0) {
    delayMicroseconds(us);
  }
  interrupts();
}



/// setup SPI, read device ID etc...
boolean SPIFlash::initialize()
{
  pinMode(_slaveSelectPin, OUTPUT);
  unselect();
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV2); //max speed, except on Due which can run at system clock speed
  SPI.begin();

  if (_jedecID == 0 || readDeviceId() == _jedecID) {
    command(SPIFLASH_STATUSWRITE, true); // Write Status Register
    SPI.transfer(0);                     // Global Unprotect
    unselect();
    return true;
  }
  return false;
}

/// Get the manufacturer and device ID bytes (as a short word)
word SPIFlash::readDeviceId()
{
#if defined(__AVR_ATmega32U4__) // Arduino Leonardo, MoteinoLeo
  command(SPIFLASH_IDREAD); // Read JEDEC ID
#else
  select();
  SPI.transfer(SPIFLASH_IDREAD);
#endif
  word jedecid = SPI.transfer(0) << 8;
  jedecid |= SPI.transfer(0);
  unselect();
  return jedecid;
}

/// Get the 64 bit unique identifier, stores it in UNIQUEID[8]. Only needs to be called once, ie after initialize
/// Returns the byte pointer to the UNIQUEID byte array
/// Read UNIQUEID like this:
/// flash.readUniqueId(); for (byte i=0;i<8;i++) { Serial.print(flash.UNIQUEID[i], HEX); Serial.print(' '); }
/// or like this:
/// flash.readUniqueId(); byte* MAC = flash.readUniqueId(); for (byte i=0;i<8;i++) { Serial.print(MAC[i], HEX); Serial.print(' '); }
byte* SPIFlash::readUniqueId()
{
  command(SPIFLASH_MACREAD);
  SPI.transfer(0);
  SPI.transfer(0);
  SPI.transfer(0);
  SPI.transfer(0);
  for (byte i=0;i<8;i++)
    UNIQUEID[i] = SPI.transfer(0);
  unselect();
  return UNIQUEID;
}

/// read 1 byte from flash memory
byte SPIFlash::readByte(long addr) {
  command(SPIFLASH_ARRAYREADLOWFREQ);
  SPI.transfer(addr >> 16);
  SPI.transfer(addr >> 8);
  SPI.transfer(addr);
  byte result = SPI.transfer(0);
  unselect();
  return result;
}

/// read unlimited # of bytes
void SPIFlash::readBytes(long addr, void* buf, word len) {
  command(SPIFLASH_ARRAYREAD);
  SPI.transfer(addr >> 16);
  SPI.transfer(addr >> 8);
  SPI.transfer(addr);
  SPI.transfer(0); //"dont care"
  for (word i = 0; i < len; ++i)
    ((byte*) buf)[i] = SPI.transfer(0);
  unselect();
}

/// Send a command to the flash chip, pass TRUE for isWrite when its a write command
void SPIFlash::command(byte cmd, boolean isWrite, boolean busyWait){
#if defined(__AVR_ATmega32U4__) // Arduino Leonardo, MoteinoLeo
  DDRB |= B00000001;            // Make sure the SS pin (PB0 - used by RFM12B on MoteinoLeo R1) is set as output HIGH!
  PORTB |= B00000001;
#endif
  if (isWrite)
  {
    command(SPIFLASH_WRITEENABLE); // Write Enable
    unselect();
  }

  if (busyWait) {
    while(busy()); //wait for any write/erase to complete
  }

  select();

  SPI.transfer(cmd);
}

/// check if the chip is busy erasing/writing
boolean SPIFlash::busy()
{
  /*
  select();
  SPI.transfer(SPIFLASH_STATUSREAD);
  byte status = SPI.transfer(0);
  unselect();
  return status & 1;
  */
  return readStatus() & 1;
}

/// return the STATUS register
byte SPIFlash::readStatus()
{
  select();
  SPI.transfer(SPIFLASH_STATUSREAD);
  byte status = SPI.transfer(0);
  unselect();
  return status;
}


/// Write 1 byte to flash memory
/// WARNING: you can only write to previously erased memory locations (see datasheet)
///          use the block erase commands to first clear memory (write 0xFFs)
void SPIFlash::writeByte(long addr, uint8_t byt) {
  command(SPIFLASH_BYTEPAGEPROGRAM, true);  // Byte/Page Program
  SPI.transfer(addr >> 16);
  SPI.transfer(addr >> 8);
  SPI.transfer(addr);
  SPI.transfer(byt);
  unselect();
}

/// write 1-256 bytes to flash memory
/// WARNING: you can only write to previously erased memory locations (see datasheet)
///          use the block erase commands to first clear memory (write 0xFFs)
/// WARNING: if you write beyond a page boundary (or more than 256bytes),
///          the bytes will wrap around and start overwriting at the beginning of that same page
///          see datasheet for more details
void SPIFlash::writeBytes(long addr, const void* buf, uint16_t len) {
  command(SPIFLASH_BYTEPAGEPROGRAM, true);  // Byte/Page Program
  SPI.transfer(addr >> 16);
  SPI.transfer(addr >> 8);
  SPI.transfer(addr);
  for (uint16_t i = 0; i < len; i++) {
    SPI.transfer(((byte*) buf)[i]);
  }
  unselect();
}

/// erase entire flash memory array
/// may take several seconds depending on size, but is non blocking
/// so you may wait for this to complete using busy() or continue doing
/// other things and later check if the chip is done with busy()
/// note that any command will first wait for chip to become available using busy()
/// so no need to do that twice
void SPIFlash::chipErase() {
  command(SPIFLASH_CHIPERASE, true);
  unselect();
}

/// erase a 4Kbyte block
void SPIFlash::blockErase4K(long addr) {
  command(SPIFLASH_BLOCKERASE_4K, true); // Block Erase
  SPI.transfer(addr >> 16);
  SPI.transfer(addr >> 8);
  SPI.transfer(addr);
  unselect();
}

/// erase a 32Kbyte block
void SPIFlash::blockErase32K(long addr) {
  command(SPIFLASH_BLOCKERASE_32K, true); // Block Erase
  SPI.transfer(addr >> 16);
  SPI.transfer(addr >> 8);
  SPI.transfer(addr);
  unselect();
}

void SPIFlash::sleep() {
  command(SPIFLASH_SLEEP); // SLEEP
  unselect();
}

void SPIFlash::wakeup() {
  // not a write command
  // do not wait for non-busy status because only SPIFLASH_WAKE command is accepted by the chip
  // when in sleep mode
  command(SPIFLASH_WAKE, false, false); 

  // drive pin high for TRES1 microseconds
  unselect(SPIFLASH_T_RES_1_US);
}

/// cleanup
void SPIFlash::end() {
  SPI.end();
}
