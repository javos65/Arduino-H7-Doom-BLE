/*****************************************************************************
* | File      	: doomgeneric_arduino.cpp
* | Function    :	doomgeneric for arduino port
* | Info        : JV 2024
*----------------
* |	This version:   V1.0
* | Date        :   2024-02-16
* | Port        :   Arduino Portenta H7 with usb-c and BLE support
* | Info        :   BLE version for remote control via Smartphone App
* | IOriginal   :   Mbed Arduino https://github.com/arduino/ArduinoCore-mbed
*
* added DoomDebug.h  : supports serial debug messages via Serial (serial3 on H7)
* added BLE_config.h : defines BLE characteristics
* adapted doomkeys.h : unify scancodes
* Remote controll App : Build in MIT App inventor https://ai2.appinventor.mit.edu/
* See Github code : https://github.com/javos65/Arduino-H7-Doom-BLE
*
*     Remark: Arduino-H7-video library support video over usb-c, but screws up the USB bus, both HS and FS bus are corrupted.
*     Untill Mbed solves this, usb-keyboard use is not possible with usb-c video running to do game control.
*
******************************************************************************/

#include "Arduino.h"
#include "mbed.h"
#include "Arduino_H7_Video.h"
#include "dsi.h"

#define sleep _sleep

#include "doomkeys.h" // JV adapted
#include "m_argv.h"
#include "doomgeneric.h"

#include <stdio.h>
#include <unistd.h>

//JV : BLE library and globals  -- start -------------------------------

#include "BLE_config.h"     // JV for BLE remote definitions
#include "DoomDebug.h"      // JV for Serial debugging
#define KEYQUEUE_SIZE 32
static uint16_t s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;
uint32_t bletick=0;

// JV : BLE and key Fifo globals  -- end --------------------------------

static int FrameBufferFd = -1;
static int* FrameBuffer = 0;


static unsigned int s_PositionX = 0;
static unsigned int s_PositionY = 0;


uint32_t LCD_X_Size = 0, LCD_Y_Size = 0;
DMA2D_HandleTypeDef    DMA2D_Handle;

extern struct color colors[];

static void InitCLUT(uint32_t * clut)
{
  uint32_t  red = 0, green = 0, blue = 0;
  uint32_t  i = 0x00;

  /* Color map generation */
  for (i = 0; i < 256; i++)
  {
    /* Generate red, green and blue values */
    red = (i * 8) % 256;
    green = (i * 6) % 256;
    blue = (i * 4) % 256;

    red = red << 16;
    green = green << 8;

    /* Store the 32-bit value */
    clut[i] = 0xFF000000 | (red + green + blue);
  }
}

uint32_t __ALIGNED(32) L8_CLUT[256];
static DMA2D_CLUTCfgTypeDef clut;

static void DMA2D_Init(uint16_t xsize, uint16_t ysize)
{
  /*##-1- Configure the DMA2D Mode, Color Mode and output offset #############*/
  DMA2D_Handle.Init.Mode         = DMA2D_M2M_PFC;
  DMA2D_Handle.Init.ColorMode    = DMA2D_OUTPUT_RGB565;
  DMA2D_Handle.Init.OutputOffset = 0; //LCD_X_Size - xsize;
  DMA2D_Handle.Init.AlphaInverted = DMA2D_REGULAR_ALPHA;  /* No Output Alpha Inversion*/
  DMA2D_Handle.Init.RedBlueSwap   = DMA2D_RB_REGULAR;     /* No Output Red & Blue swap */

  /*##-2- DMA2D Callbacks Configuration ######################################*/
  DMA2D_Handle.XferCpltCallback  = NULL;

  /*##-3- Foreground Configuration ###########################################*/
  DMA2D_Handle.LayerCfg[1].AlphaMode = DMA2D_REPLACE_ALPHA; //DMA2D_NO_MODIF_ALPHA;
  DMA2D_Handle.LayerCfg[1].InputAlpha = 0x00;
  DMA2D_Handle.LayerCfg[1].InputColorMode = DMA2D_INPUT_L8; //DMA2D_OUTPUT_RGB565;
  //DMA2D_Handle.LayerCfg[1].ChromaSubSampling = cssMode;
  DMA2D_Handle.LayerCfg[1].InputOffset = 0; //LCD_Y_Size - ysize;
  DMA2D_Handle.LayerCfg[1].RedBlueSwap = DMA2D_RB_REGULAR; /* No ForeGround Red/Blue swap */
  DMA2D_Handle.LayerCfg[1].AlphaInverted = DMA2D_REGULAR_ALPHA; /* No ForeGround Alpha inversion */

  DMA2D_Handle.Instance          = DMA2D;

  /*##-4- DMA2D Initialization     ###########################################*/
  HAL_DMA2D_Init(&DMA2D_Handle);
  HAL_DMA2D_ConfigLayer(&DMA2D_Handle, 1);

  memcpy(L8_CLUT, colors, 256 * 4);
  clut.pCLUT = (uint32_t *)L8_CLUT; //(uint32_t *)colors;
  clut.CLUTColorMode = DMA2D_CCM_ARGB8888;
  clut.Size = 0xFF;

#ifdef CORE_CM7
  SCB_CleanInvalidateDCache();
  SCB_InvalidateICache();
  //SCB_InvalidateDCache_by_Addr(clut.pCLUT, clut.Size);
#endif

  HAL_DMA2D_CLUTLoad(&DMA2D_Handle, clut, 1);
  HAL_DMA2D_PollForTransfer(&DMA2D_Handle, 100);
}

Arduino_H7_Video display(640, 480, USBCVideo);

uint32_t fbs[2];

void DG_Init()
{

  DBSERIALPORT.begin(38400);                                 // JV init DEBUG serial port for key enty
  Debugln("Serial initialized");        
                  
  pinMode(LEDG, OUTPUT);digitalWrite(LEDG, LOW);            // JV set green on
  display.begin();                                          // JV init display, green off
  digitalWrite(LEDG, HIGH); Debugln("Display initialized");  

  //usb.Init(USB_CORE_ID_HS, class_table);
  pinMode(LEDB, OUTPUT);digitalWrite(LEDB, LOW);            // JV set blue on
  if (!initBLE()) {
    Debugln("Starting Bluetooth® Low Energy failed!");
    while (1){                                              // Stop if Bluetooth® Low Energy couldn't be initialized.
          digitalWrite(LEDR, LOW); delay(500);
          digitalWrite(LEDR, HIGH); delay(500);
          }
    }      
  digitalWrite(LEDB, HIGH); Debugln("BLE initialized");   
  bletick=millis();                                        // JV set bletick - time ticker
}

void DG_OnPaletteReload() {
  DMA2D_Init(DOOMGENERIC_RESX, DOOMGENERIC_RESY);
}


//#define DEBUG_CM7_VIDEO

static void DMA2D_CopyBuffer(uint32_t *pSrc, uint32_t *pDst)
{
  uint32_t xPos, yPos, destination;

  /*##-1- calculate the destination transfer address  ############*/
  xPos = (display.width() - DOOMGENERIC_RESX) / 2;
  yPos = (display.height() - DOOMGENERIC_RESY) / 2;

  destination = (uint32_t)pDst; // + ((yPos * stm32_getXSize()) + xPos) * 4;

  HAL_DMA2D_PollForTransfer(&DMA2D_Handle, 100);  /* wait for the previous DMA2D transfer to ends */
  /* copy the new decoded frame to the LCD Frame buffer*/
  HAL_DMA2D_Start(&DMA2D_Handle, (uint32_t)pSrc, destination, DOOMGENERIC_RESX, DOOMGENERIC_RESY);
#if defined(CORE_CM7) && !defined(DEBUG_CM7_VIDEO) 
  HAL_DMA2D_PollForTransfer(&DMA2D_Handle, 100);  /* wait for the previous DMA2D transfer to ends */
#endif
}

void DG_DrawFrame()
{
  uint32_t fb = dsi_getCurrentFrameBuffer();
#ifdef CORE_CM7
  //SCB_CleanInvalidateDCache();
  //SCB_InvalidateICache();
  SCB_InvalidateDCache_by_Addr((uint32_t *)fb, DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
#endif
  DMA2D_CopyBuffer((uint32_t *)DG_ScreenBuffer, (uint32_t *)fb);
  dsi_drawCurrentFrameBuffer();
  check_BLE_keys();                                     // JV check BLE key reception -----------------------------------
}

void DG_SleepMs(uint32_t ms)
{
  delay(ms);
}

uint32_t DG_GetTicksMs()
{
  return millis();
}

void DG_SetWindowTitle(const char * title)
{
}





// JV BLE Keyboard convert functions for customized DG_GetKey()
// BLE check for connection and received character
// BLE sends complete Doom keys in 16 bit uint : bit[15:0] = Doomkey , bit[31:16] = pressed
static uint8_t check_BLE_keys()
{
  uint8_t status=0;
  uint16_t v;
  uint32_t t=millis();
  if(t-bletick > 200){ // JV can only once every 200 ms
    bletick = t;
    BLEcentral = BLE.central();
    if (BLEcentral) {
      digitalWrite(LEDB, LOW);
      if (doomCharacteristic.written()) {
          v= doomCharacteristic.value();          // JV read ble scancode 16 bit
          s_KeyQueue[s_KeyQueueWriteIndex] =  v; 
          s_KeyQueueWriteIndex++;
          s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
          {Debug("[");Debug(v,HEX);Debugln("]");} // JV Debug message: sends received key code
          status=1;
      }
    } // bleCentral ?
      else {
        digitalWrite(LEDB, HIGH);
        }    
  }
return status;
}


int DG_GetKey(int* pressed, unsigned char* doomKey)
{

  if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex) { //  JVfifo is empty
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
      BLE.setLocalName(BLENAME);                          // Set advertised local name and service UUID:
      BLE.setAdvertisedService(doomService);
      doomService.addCharacteristic(doomCharacteristic);  // Add the characteristic to the service : 32bit read or write
      BLE.addService(doomService);                        // Add service
      doomCharacteristic.writeValue(0);                   // Set the initial value for the characeristic = 0, no key
      BLE.advertise();                                    // start advertising
      return 1;
      }
  else return 0;                                
}