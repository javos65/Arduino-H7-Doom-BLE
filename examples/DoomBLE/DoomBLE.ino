/*
    Arduino wrapper for DoomGeneric
    Mouse and keyboard controls are not implemented at the moment. (JV !)

    To use the internal QSPI flash as storage, run QSPIFormat
    sketch once to create the partitions, AccessFlashAsUSBDisk to expose the QSPI flash
    as a USB disk, copy DOOM1.WAD in the biggest partition, flash this sketch and you are ready to go :)

  Update 2024 : JV for Portenta H7
    - Added BLE support for keyboard control via SmartPhone
    - Added Debug support via extra serial uart
    - Added Color led feedback.

    Remark: Arduino-H7-video library support video over usb-c, but screws up the USB bus, both HS and FS bus are corrupted.
    Untill Mbed solves this, usb-keyboard use is not possible with usb-c video running.
*/

#include "Arduino.h"
#include "stdio.h"

#include "QSPIFBlockDevice.h"
#include "FATFileSystem.h"
#include "MBRBlockDevice.h"
#include "doomgeneric.h"

QSPIFBlockDevice block_device;
// Comment previous line and uncomment these two if you want to store DOOM.WAD in an external SD card (FAT32 formatted)
// #include "SDMMCBlockDevice.h"
// SDMMCBlockDevice block_device;

mbed::MBRBlockDevice fs_data(&block_device, 2);
static mbed::FATFileSystem fs("fs");

extern "C" int main_wrapper(int argc, char **argv);
char*argv[] = {"/fs/doom", "-iwad", "/fs/DOOM1.WAD"};

REDIRECT_STDOUT_TO(Serial3)     // JV Console DEBUG for stdio info from Doom library - need to initialise Serial3 in setup()

void setup() {
  Serial3.begin(38400);         // JV needed for debug interface can 
  pinMode(LED_BUILTIN, OUTPUT);  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(LEDR, OUTPUT); digitalWrite(LEDR, HIGH);


  int err =  fs.mount(&fs_data);
  if (err) {
    printf("No filesystem found, please run AccessFlashAsUSBDisk sketch and copy DOOM1.WAD in the big partition");
    while (1) {
      digitalWrite(LED_BUILTIN, LOW); delay(200); // signal error fast flashing
      digitalWrite(LED_BUILTIN, HIGH); delay(200);
    }
  }
  DIR *dir;
  struct dirent *ent;
  printf("try to open dir\n");
  if ((dir = opendir("/fs")) != NULL) {
    /* print all the files and directories within directory */
    while ((ent = readdir (dir)) != NULL) {
      printf ("%s\n", ent->d_name);
    }
    closedir (dir);
  } else {
    /* could not open directory */
    printf ("error\n");
    while (1) {
      digitalWrite(LED_BUILTIN, LOW); delay(500); //signal error moderate flashing
      digitalWrite(LED_BUILTIN, HIGH); delay(500);  
    }  
  }
  digitalWrite(LEDR, LOW); delay(1500); // red on - off = start of game
  digitalWrite(LEDR, HIGH);
  main_wrapper(3, argv);  // starts Doom
}

void loop() {
  // put your main code here, to run repeatedly:
    while (1) {
      digitalWrite(LED_BUILTIN, LOW); delay(1000);    // JV signal end of program slow flashing
      digitalWrite(LED_BUILTIN, HIGH); delay(1000);    
      }
}


