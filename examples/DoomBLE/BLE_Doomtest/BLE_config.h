
/*****************************************************************************
* | File      	:	BLE_config.h.h
* | Function    :	debug with Serial print and println 
* | Info        : 	JV 2024
*----------------
* |	This version:   V1.0
* | Date        :   2024-02-16
* | Info        :   Basic version defines BKE UUID and BLEDEvice
*                   Using standard ArduinoBLE library
******************************************************************************/


#include <ArduinoBLE.h>

#define BLENAME "Doom-Remote"
BLEService doomService("19b10000-e8f2-537e-4f6c-deadbeaf0001");             // Doom wants DEADBEAF 
BLEUnsignedIntCharacteristic doomCharacteristic("19b10001-e8f2-537e-4f6c-deadbeaf0001", BLERead | BLEWrite);
// Bluetooth® Low Energy Doom Remote Characteristic - custom 128-bit UUID, read and writable by 16 bit 

BLEDevice BLEcentral; 

static uint8_t initBLE();
static uint8_t check_BLE_keys();