#include "esp_task_wdt.h"
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "ScreenIcon.h"
#include <Wire.h>   // https://github.com/rambo/I2C

#define _cs   4  // goes to TFT CS
#define _dc   0  // goes to TFT DC
#define _mosi 23  // goes to TFT MOSI
#define _sclk 18  // goes to TFT SCK/CLK
#define _rst  5   // goes to TFT RESET
#define _miso     // Not connected
//       3.3V     // Goes to TFT LED  
//       5v       // Goes to TFT Vcc
//       Gnd      // Goes to TFT Gnd

typedef struct Data_t{
  int16_t x;
  int16_t y;
  int16_t z;
};

typedef union I2C_Packet_t{
 Data_t data;
 byte I2CPacket[sizeof(Data_t)];
};  

#define PACKET_SIZE sizeof(Data_t)

typedef struct DataPrint_t{
  char buf[1024];
  int16_t x;
  int16_t y;
  uint8_t TextSize;
  uint16_t TextColor;
  bool Cord;
  bool Text;
  bool Color;
  bool Ln;
};

typedef struct DataRect_t{
  uint16_t x;
  uint16_t y;
  uint16_t w;
  uint16_t h;
  uint16_t color;
  bool fill;
};

typedef struct DataStatus_t{
  uint8_t GPS;
  uint8_t GSM;
};

const uint8_t addrSlaveI2C = 21;    // ID of I2C slave.  Don't use #define

// Function prototype
void getData();

// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 150
#define TS_MINY 100
#define TS_MAXX 920
#define TS_MAXY 900

#define MINPRESSURE 10
#define MAXPRESSURE 1000

QueueHandle_t TouchQueue;
QueueHandle_t PrintQueue;
QueueHandle_t RectQueue;
QueueHandle_t StatusQueue;

Adafruit_ILI9341 tft = Adafruit_ILI9341(_cs, _dc, _rst);

void TouchScreen_proc( void * parameter );
void TouchScreen_Update( void * parameter );
void SetupLCD();

void tftPrintLn(char* buf);
void tftPrintLn(char* buf, int16_t x, int16_t y);
void tftPrintLn(char* buf, uint8_t TextSize);
void tftPrintLn(char* buf, uint16_t TextColor);
void tftPrintLn(char* buf, uint8_t TextSize, uint16_t TextColor);
void tftPrintLn(char* buf, int16_t x, int16_t y, uint8_t TextSize);
void tftPrintLn(char* buf, int16_t x, int16_t y, uint16_t TextColor);
void tftPrintLn(char* buf, int16_t x, int16_t y, uint8_t TextSize, uint16_t TextColor);

void tftPrint(char* buf);
void tftPrint(char* buf, int16_t x, int16_t y);
void tftPrint(char* buf, uint8_t TextSize);
void tftPrint(char* buf, uint16_t TextColor);
void tftPrint(char* buf, uint8_t TextSize, uint16_t TextColor);
void tftPrint(char* buf, int16_t x, int16_t y, uint8_t TextSize);
void tftPrint(char* buf, int16_t x, int16_t y, uint16_t TextColor);
void tftPrint(char* buf, int16_t x, int16_t y, uint8_t TextSize, uint16_t TextColor);

void tftdrawRect(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t color);
void tftfillRect(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t color);

void SetupLCD(){
  PrintQueue = xQueueCreate(20, sizeof(struct DataPrint_t) );
  RectQueue = xQueueCreate(20, sizeof(struct DataRect_t) );
  StatusQueue = xQueueCreate(6, sizeof(struct DataStatus_t) );
  
  tft.begin(80000000);
  tft.setRotation(3);
  tft.fillScreen(ILI9341_WHITE); 
  tft.fillRect(0,0, tft.width(), 16, ILI9341_BLACK);
  tft.setCursor(0, 60);
  //tft.setTextSize(3);
  //tft.println("Starting..."); 

  /*xTaskCreate(
                    TouchScreen_proc,
                    "TouchScreen_proc",
                    1000,
                    NULL,
                    20,
                    NULL);*/
  xTaskCreate(
                    TouchScreen_Update,
                    "TouchScreen_Update",
                    10000,
                    NULL,
                    50,
                    NULL);
}

void TouchScreen_Update( void * parameter )
{
  String taskMessage = "TouchScreen_Update Task running on core ";
  taskMessage = taskMessage + xPortGetCoreID();
  Serial.println(taskMessage);
  DataPrint_t printlntemp;
  DataRect_t Recttemp;
  DataStatus_t Statustemp;
  
  TickType_t xDelay = 1 / portTICK_PERIOD_MS;

  uint8_t timeupdate = rtc_minute;
  uint8_t lastupdate = rtc_second, gsmstatus = 3, gpsstatus = 3;
  //I2C_Packet_t leakinfo;  

  
  
  while(true){
    
    if(rtc_minute != timeupdate){
      char buf[6];
      snprintf(buf, sizeof(buf), "%02d:%02d", rtc_hour, rtc_minute);
    
      tft.setCursor(tft.width()-60, 0);
      tft.fillRect(tft.width()-60,0, 60, 16, ILI9341_BLACK);
      tft.setTextSize(2);
      tft.setTextColor(ILI9341_WHITE);
      tft.println(buf);
      timeupdate = rtc_minute;
    }
    
    if(uxQueueMessagesWaiting(StatusQueue) != 0){
      xQueueReceive(StatusQueue, &Statustemp, 0);
      gpsstatus = Statustemp.GPS;
      gsmstatus = Statustemp.GSM;
      if(gpsstatus == 0){
        tft.drawBitmap(0, 0, GPSICON, 16, 16, ILI9341_GREEN);
      }
      if(gsmstatus == 0){
        tft.drawBitmap(16, 0, GSMICON, 16, 16, ILI9341_GREEN);
      }
    }
    if(gpsstatus != 0 || gsmstatus != 0){
      if(lastupdate != rtc_second){
        if (rtc_second%2 == 0){
          if(gpsstatus == 1){
            tft.drawBitmap(0, 0, GPSICON, 16, 16, ILI9341_YELLOW);
          }
          if(gsmstatus == 1){
            tft.drawBitmap(16, 0, GSMICON, 16, 16, ILI9341_YELLOW);
          }
        }else{
          if(gpsstatus == 1){
            tft.drawBitmap(0, 0, GPSICON, 16, 16, ILI9341_LIGHTGREY);
          }
          if(gsmstatus == 1){
            tft.drawBitmap(16, 0, GSMICON, 16, 16, ILI9341_LIGHTGREY);
          }
        }
        if (rtc_second%3 == 0){
          if(gpsstatus == 2){
            tft.drawBitmap(0, 0, GPSICON, 16, 16, ILI9341_LIGHTGREY);
          }
          if(gsmstatus == 2){
            tft.drawBitmap(16, 0, GSMICON, 16, 16, ILI9341_LIGHTGREY);
          }
        }else{
          if(gpsstatus == 2){
            tft.drawBitmap(0, 0, GPSICON, 16, 16, ILI9341_RED);
          }
          if(gsmstatus == 2){
            tft.drawBitmap(16, 0, GSMICON, 16, 16, ILI9341_RED);
          }
        }
        if(gpsstatus == 3){
          tft.drawBitmap(0, 0, GPSICON, 16, 16, ILI9341_LIGHTGREY);
        }
        if(gsmstatus == 3){
          tft.drawBitmap(16, 0, GSMICON, 16, 16, ILI9341_LIGHTGREY);
        }
        lastupdate = rtc_second;
      }
    }
    
    
    if(uxQueueMessagesWaiting(RectQueue) != 0){
      xQueueReceive(RectQueue, &Recttemp, 0);
      if(Recttemp.fill == true){
        tft.fillRect(Recttemp.x, Recttemp.y, Recttemp.w, Recttemp.h, Recttemp.color);
      }else{
        tft.drawRect(Recttemp.x, Recttemp.y, Recttemp.w, Recttemp.h, Recttemp.color);
      }
    }
    if(uxQueueMessagesWaiting(PrintQueue) != 0){
      xQueueReceive(PrintQueue, &printlntemp, 0);
      
      if(printlntemp.Cord == true){
        tft.setCursor(printlntemp.x, printlntemp.y);
      }
      if(printlntemp.Text == true){
        tft.setTextSize(printlntemp.TextSize);
      }
      if(printlntemp.Color == true){
        tft.setTextColor(printlntemp.TextColor);
      }
      if(printlntemp.Ln == true){
        tft.println(printlntemp.buf);
      }else{
        tft.print(printlntemp.buf);
      }
      
    }
    vTaskDelay(xDelay);
  }  
  vTaskDelete( NULL );
}

void TouchScreen_proc( void * parameter ){
    String taskMessage = "TouchScreen_proc Task running on core ";
    taskMessage = taskMessage + xPortGetCoreID();
    Serial.println(taskMessage);
  
    TouchQueue = xQueueCreate( 1, sizeof( struct Data_t ) );
  
    pinMode(25, OUTPUT);
    digitalWrite(25, HIGH);
    
    pinMode(SDA, INPUT); // Make SDA (data) and SCL (clock) pins Inputs with pullup.
    pinMode(SCL, INPUT);
    pinMode(SDA, INPUT_PULLUP); // Make SDA (data) and SCL (clock) pins Inputs with pullup.
    pinMode(SCL, INPUT_PULLUP);

    Wire.begin();
    Wire.setClock(10000);
  
    TickType_t xDelay = 1 / portTICK_PERIOD_MS;
    TickType_t xDelay500 = 500 / portTICK_PERIOD_MS;
    TickType_t xDelay1000 = 1000 / portTICK_PERIOD_MS;
    
    I2C_Packet_t leakinfotemp; 
    byte byteArray[PACKET_SIZE];

    int i = 0;
    int n;

    vTaskDelay(xDelay500);
    
    while(true){
      xDelay = 1 / portTICK_PERIOD_MS;
      if(uxQueueSpacesAvailable(TouchQueue) > 0){
        n = Wire.requestFrom(addrSlaveI2C, PACKET_SIZE);
        Wire.readBytes( byteArray, PACKET_SIZE);
        if(n != 0){
          for (int k=0; k < PACKET_SIZE; k++)
          { 
            leakinfotemp.I2CPacket[k] = byteArray[k];
          }
          if(leakinfotemp.data.z <= 0){
            if(i > 20){
              xDelay = 50 / portTICK_PERIOD_MS;
            }else{
              i++;
            }
          }else if(i != 0){
            i = 0;
          }
          Serial.println("i2c r packet:");
          Serial.println(leakinfotemp.data.x);
          Serial.println(leakinfotemp.data.y);
          Serial.println(leakinfotemp.data.z);
          xQueueSend(TouchQueue, &leakinfotemp.I2CPacket, 0);
        }else{
          Serial.println("reboot i2c controller");
          digitalWrite(25, LOW);
          vTaskDelay(xDelay1000);
          digitalWrite(25, HIGH);
          vTaskDelay(xDelay1000);
        }
        
      }
      vTaskDelay(xDelay);
    }
    vTaskDelete( NULL );
}

void tftPrintLn(char* buf){
  DataPrint_t data;

  memset(data.buf, '\0', 1024);
  
  strcpy(data.buf, buf);
  data.Cord = false;
  data.Text = false;
  data.Color = false;
  data.Ln = true;

  if(uxQueueSpacesAvailable(PrintQueue) > 0){
    xQueueSend(PrintQueue, &data, 0);
  }
}
void tftPrintLn(char* buf, int16_t x, int16_t y){
  DataPrint_t data;

  memset(data.buf, '\0', 1024);
  
  strcpy(data.buf, buf);
  data.x = x;
  data.y = y;
  data.Cord = true;
  data.Text = false;
  data.Color = false;
  data.Ln = true;

  if(uxQueueSpacesAvailable(PrintQueue) > 0){
    xQueueSend(PrintQueue, &data, 0);
  }
}
void tftPrintLn(char* buf, int16_t x, int16_t y, uint8_t TextSize){
  DataPrint_t data;

  memset(data.buf, '\0', 1024);
  
  strcpy(data.buf, buf);
  data.x = x;
  data.y = y;
  data.TextSize = TextSize;
  data.Cord = true;
  data.Text = true;
  data.Color = false;
  data.Ln = true;

  if(uxQueueSpacesAvailable(PrintQueue) > 0){
    xQueueSend(PrintQueue, &data, 0);
  }
}
void tftPrintLn(char* buf, int16_t x, int16_t y, uint8_t TextSize, uint16_t TextColor){
  DataPrint_t data;

  memset(data.buf, '\0', 1024);
  
  strcpy(data.buf, buf);
  data.x = x;
  data.y = y;
  data.TextSize = TextSize;
  data.TextColor = TextColor;
  data.Cord = true;
  data.Text = true;
  data.Color = true;
  data.Ln = true;

  if(uxQueueSpacesAvailable(PrintQueue) > 0){
    xQueueSend(PrintQueue, &data, 0);
  }
}
void tftPrintLn(char* buf, uint8_t TextSize){
  DataPrint_t data;

  memset(data.buf, '\0', 1024);
  
  strcpy(data.buf, buf);
  data.TextSize = TextSize;
  data.Cord = false;
  data.Text = true;
  data.Color = false;
  data.Ln = true;

  if(uxQueueSpacesAvailable(PrintQueue) > 0){
    xQueueSend(PrintQueue, &data, 0);
  }
}
void tftPrintLn(char* buf, uint8_t TextSize, uint16_t TextColor){
  DataPrint_t data;

  memset(data.buf, '\0', 1024);
  
  strcpy(data.buf, buf);
  data.TextSize = TextSize;
  data.TextColor = TextColor;
  data.Cord = false;
  data.Text = true;
  data.Color = true;
  data.Ln = true;

  if(uxQueueSpacesAvailable(PrintQueue) > 0){
    xQueueSend(PrintQueue, &data, 0);
  }
}
void tftPrintLn(char* buf, int16_t x, int16_t y, uint16_t TextColor){
  DataPrint_t data;

  memset(data.buf, '\0', 1024);
  
  strcpy(data.buf, buf);
  data.x = x;
  data.y = y;
  data.TextColor = TextColor;
  data.Cord = true;
  data.Text = false;
  data.Color = true;
  data.Ln = true;

  if(uxQueueSpacesAvailable(PrintQueue) > 0){
    xQueueSend(PrintQueue, &data, 0);
  }
}
void tftPrintLn(char* buf, uint16_t TextColor){
  DataPrint_t data;

  memset(data.buf, '\0', 1024);
  
  strcpy(data.buf, buf);
  data.TextColor = TextColor;
  data.Cord = false;
  data.Text = true;
  data.Color = false;
  data.Ln = true;

  if(uxQueueSpacesAvailable(PrintQueue) > 0){
    xQueueSend(PrintQueue, &data, 0);
  }
}

void tftPrint(char* buf){
  DataPrint_t data;

  memset(data.buf, '\0', 1024);
  
  strcpy(data.buf, buf);
  data.Cord = false;
  data.Text = false;
  data.Color = false;
  data.Ln = false;

  if(uxQueueSpacesAvailable(PrintQueue) > 0){
    xQueueSend(PrintQueue, &data, 0);
  }
}
void tftPrint(char* buf, int16_t x, int16_t y){
  DataPrint_t data;

  memset(data.buf, '\0', 1024);
  
  strcpy(data.buf, buf);
  data.x = x;
  data.y = y;
  data.Cord = true;
  data.Text = false;
  data.Color = false;
  data.Ln = false;

  if(uxQueueSpacesAvailable(PrintQueue) > 0){
    xQueueSend(PrintQueue, &data, 0);
  }
}
void tftPrint(char* buf, int16_t x, int16_t y, uint8_t TextSize){
  DataPrint_t data;

  memset(data.buf, '\0', 1024);
  
  strcpy(data.buf, buf);
  data.x = x;
  data.y = y;
  data.TextSize = TextSize;
  data.Cord = true;
  data.Text = true;
  data.Color = false;
  data.Ln = false;

  if(uxQueueSpacesAvailable(PrintQueue) > 0){
    xQueueSend(PrintQueue, &data, 0);
  }
}
void tftPrint(char* buf, int16_t x, int16_t y, uint8_t TextSize, uint16_t TextColor){
  DataPrint_t data;

  memset(data.buf, '\0', 1024);
  
  strcpy(data.buf, buf);
  data.x = x;
  data.y = y;
  data.TextSize = TextSize;
  data.TextColor = TextColor;
  data.Cord = true;
  data.Text = true;
  data.Color = true;
  data.Ln = false;

  if(uxQueueSpacesAvailable(PrintQueue) > 0){
    xQueueSend(PrintQueue, &data, 0);
  }
}
void tftPrint(char* buf, uint8_t TextSize){
  DataPrint_t data;

  memset(data.buf, '\0', 1024);
  
  strcpy(data.buf, buf);
  data.TextSize = TextSize;
  data.Cord = false;
  data.Text = true;
  data.Color = false;
  data.Ln = false;

  if(uxQueueSpacesAvailable(PrintQueue) > 0){
    xQueueSend(PrintQueue, &data, 0);
  }
}
void tftPrint(char* buf, uint8_t TextSize, uint16_t TextColor){
  DataPrint_t data;

  memset(data.buf, '\0', 1024);
  
  strcpy(data.buf, buf);
  data.TextSize = TextSize;
  data.TextColor = TextColor;
  data.Cord = false;
  data.Text = true;
  data.Color = true;
  data.Ln = false;

  if(uxQueueSpacesAvailable(PrintQueue) > 0){
    xQueueSend(PrintQueue, &data, 0);
  }
}
void tftPrint(char* buf, int16_t x, int16_t y, uint16_t TextColor){
  DataPrint_t data;

  memset(data.buf, '\0', 1024);
  
  strcpy(data.buf, buf);
  data.x = x;
  data.y = y;
  data.TextColor = TextColor;
  data.Cord = true;
  data.Text = false;
  data.Color = true;
  data.Ln = false;

  if(uxQueueSpacesAvailable(PrintQueue) > 0){
    xQueueSend(PrintQueue, &data, 0);
  }
}
void tftPrint(char* buf, uint16_t TextColor){
  DataPrint_t data;

  memset(data.buf, '\0', 1024);
  
  strcpy(data.buf, buf);
  data.TextColor = TextColor;
  data.Cord = false;
  data.Text = true;
  data.Color = false;
  data.Ln = false;

  if(uxQueueSpacesAvailable(PrintQueue) > 0){
    xQueueSend(PrintQueue, &data, 0);
  }
}

void tftdrawRect(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t color){
  DataRect_t data;

  data.x = x0;
  data.y = y0;
  data.w = w;
  data.h = h;
  data.color = color;
  data.fill = false;
  
  if(uxQueueSpacesAvailable(RectQueue) > 0){
    xQueueSend(RectQueue, &data, 0);
  }
}
void tftfillRect(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t color){
  DataRect_t data;

  data.x = x0;
  data.y = y0;
  data.w = w;
  data.h = h;
  data.color = color;
  data.fill = true;
  
  if(uxQueueSpacesAvailable(RectQueue) > 0){
    xQueueSend(RectQueue, &data, 0);
  }
}

