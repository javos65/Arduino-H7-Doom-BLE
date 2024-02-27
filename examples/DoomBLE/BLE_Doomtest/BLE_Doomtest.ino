/*****************************************************************************
* | File      	:	BLE_doomtest.ino
* | Function    :	Arduino script for BLE remote key testing
* | Info        : JV 2024
*----------------
* |	This version:   V1.0
* | Date        :   2024-02-16
* | Target      :   Arduino Portenta H7 - usb-c + BLE support
* | Info        :   Basic version
*
******************************************************************************/

#include "BLE_config.h"
#include "DoomDebug.h"          // defines sErial3 as debug out
#include "doomkeys.h"

REDIRECT_STDOUT_TO(Serial3)     // redirect for Stdio debug messages from Doom library over Serial3 (extra uart)

uint32_t bletick=0;

#define KEYQUEUE_SIZE 48
static uint16_t s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

void setup()
{
  Serial.begin(115200);
  DBSERIALPORT.begin(38400);                           // JV init serial port for debugging
  Debugln("Serial initialized");        

  pinMode(LEDR, OUTPUT);digitalWrite(LEDR, HIGH);      // JV set red off
  pinMode(LEDG, OUTPUT);digitalWrite(LEDG, HIGH);      // JV set green off
  pinMode(LEDB, OUTPUT);digitalWrite(LEDB, LOW);       // JV set blue on

  if (!initBLE()) {
    Debugln("Starting Bluetooth® Low Energy failed!");
    while (1){                                        // Stop if Bluetooth® Low Energy couldn't be initialized.
          digitalWrite(LEDR, LOW); delay(500);
          digitalWrite(LEDR, HIGH); delay(500);
          }
    }      
  digitalWrite(LEDB, HIGH); Debugln("BLE initialized");                                        
}

void loop() {
  int pressed;
  unsigned char doomKey;
  check_BLE_keys();

 if ( DG_GetKey(&pressed, &doomKey) ){
  Serial.print("Press[");Serial.print(pressed,HEX); 
  Serial.print("],Doomkey[");Serial.print(doomKey,HEX);
  Serial.println("]");
  }
}


// JV BLE Doom Keyboard convert functions
// BLE check for conenction and received character
// BLE sends complete Doom keys in 16 bit uint : bit[15:0] = Doomkey bit[31:16] = pressed
static uint8_t check_BLE_keys()
{
  uint8_t status=0;
  uint16_t v;
  uint32_t t=millis();
  if(t-bletick > 200){ // scan only once every 200 ms
    bletick = t;
    BLEcentral = BLE.central();
    if (BLEcentral) {
      digitalWrite(LEDB, LOW);
      if (doomCharacteristic.written()) {
          v= doomCharacteristic.value();          // read ble scancode 16 bit
          s_KeyQueue[s_KeyQueueWriteIndex] =  v;  
          s_KeyQueueWriteIndex++;
          s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
          {Debug("[");Debug(v,HEX);Debugln("]");} // debug message shows received key
          status=1;
      }
    } // bleCentral 
      else {
        digitalWrite(LEDB, HIGH);
        }    
  }
return status;
}


int DG_GetKey(int* pressed, unsigned char* doomKey)
{

  if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex) { // fifo is empty
    return 0 ;
  }
  else{
    uint16_t x = s_KeyQueue[s_KeyQueueReadIndex];
    *doomKey = (unsigned char) (x & 0x00ff);
    *pressed = (int) (x>>8) & 0x0001;
    s_KeyQueueReadIndex ++;
    s_KeyQueueReadIndex %= KEYQUEUE_SIZE;
    return 1; 
  }
}

static uint8_t initBLE(){
  if (BLE.begin()) {
      BLE.setLocalName(BLENAME);                 // Set advertised local name and service UUID:
      BLE.setAdvertisedService(doomService);
      doomService.addCharacteristic(doomCharacteristic);  // Add the characteristic to the service : 32bit read or write
      BLE.addService(doomService);                        // Add service
      doomCharacteristic.writeValue(0);                   // Set the initial value for the characeristic = 0, no key
      BLE.advertise();                                    // start advertising
      return 1;
      }
  else return 0;                                
}