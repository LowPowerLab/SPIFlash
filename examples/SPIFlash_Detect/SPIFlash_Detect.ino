// Blink LED_BUILTIN slower if the FLASH chip is detected, and faster if there is no FLASH chip detected
// Code by Felix Rusu, LowPowerLab.com
#include <SPIFlash.h>   //https://github.com/lowpowerlab/spiflash
#include <SPI.h>

#define SERIAL_BAUD      115200
#define BLINK_FAST_DELAY 50
#define BLINK_SLOW_DELAY 1000

SPIFlash flash(SS_FLASHMEM, 0xEF30); //EF40 for 16mbit windbond chip
int LEDTIME = 500;

// the setup routine runs once when you press reset:
void setup() {
#ifdef SERIAL_BAUD
  Serial.begin(SERIAL_BAUD);
  while (!Serial) delay(100); //wait until Serial/monitor is opened
#endif
  pinMode(LED_BUILTIN, OUTPUT);

  //ensure the radio module CS pin is pulled HIGH or it might interfere!
  pinMode(SS, OUTPUT); digitalWrite(SS, HIGH);

  //ensure FLASH chip is not sleep mode (unresponsive)
  flash.wakeup();
  if (flash.initialize()) {
    Serial.println("SPI Flash Init OK!");
    LEDTIME = BLINK_SLOW_DELAY;
  } else {
    Serial.println("SPI Flash Init FAIL! (is chip soldered?)");
    LEDTIME = BLINK_FAST_DELAY;
  }
}

// the loop routine runs over and over again forever:
void loop() {
  word jedecid = flash.readDeviceId();
#ifdef SERIAL_ENABLE
  Serial.print("FLASH DeviceID: ");
  Serial.println(jedecid, HEX);
  Serial.print("FLASH init "); Serial.println(flash.initialize() ? "OK" : "FAIL");
#endif    
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(LEDTIME);               // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(LEDTIME);               // wait for a second
}