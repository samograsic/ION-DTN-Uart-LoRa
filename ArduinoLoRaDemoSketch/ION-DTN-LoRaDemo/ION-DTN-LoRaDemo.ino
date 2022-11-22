//This is a minimal demo sketch for ION-UART CLA. Current LoRa Semtech pin configuration is set for Adafruit LoRa Feather M0 development 
//boar Note that this sketch does not take care of flow control, CRC, tx duty cycle etc.
//Samo Grasic (samo@grasic.net)
#include <SPI.h>              
#include "LoRaSem.h"

#define LORABUFLENGTH 255
#define SERIALTIMEOUT 100
#define FREQUENCY 434E6

const int csPin = 8;          // LoRa radio chip select
const int resetPin = 4;       // LoRa radio reset
const int irqPin = 3;         // change for your board; must be a hardware interrupt pin
uint8_t  outgoing[LORABUFLENGTH];              // outgoing message
uint8_t  incomming[LORABUFLENGTH];              // outgoing message

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(SERIALTIMEOUT);
  while (!Serial);
  Serial.println("LoRa ION Demo Arduino Sketch");
  LoRa.setPins(csPin, resetPin, irqPin);// set CS, reset, IRQ pin
  if (!LoRa.begin(FREQUENCY)) {             // initialize ratio at 434 MHz
    Serial.println("LoRa init failed. Check your pin settings.");
    while (true);                       // if failed, do nothing
  }
  Serial.println("LoRa init succeeded.");
  LoRa.setSpreadingFactor(9); 
  LoRa.setSignalBandwidth(250E3);
  LoRa.setCodingRate4(5); 
  LoRa.setTxPower(20); 
  LoRa.enableCrc(); 
  LoRa.receive();
}

void loop() 
{
  if (Serial.available()) 
  {
    int readBytes=Serial.readBytes((char*)incomming, LORABUFLENGTH);
    LoRa.beginPacket();    
    LoRa.write(incomming, readBytes);               
    LoRa.endPacket();                     
  }
  int packetSize = LoRa.parsePacket();
  if (packetSize) 
  {
    int i=0;
    while(LoRa.available()&&(i<LORABUFLENGTH)) 
    {
      outgoing[i]=LoRa.read();
      i++;
    }
    Serial.write(outgoing, i);
  }
}
