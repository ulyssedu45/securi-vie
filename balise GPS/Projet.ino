#include "esp_system.h"
#include "WiFi.h"
#include "Time.h"
#include "SDCARD.h"
#include "Vibreur.h"
#include "Fonction.h"
#include "TouchScreen_proc.h"    
#include <ArduinoJson.h>

HardwareSerial Serial2(2);

int8_t answer;

const int _rstpin = 2;

char aux_string[100];
char pin[] = "8522";
char apn[] = "sl2sfr";
//char apn[] = "free";

const char* ssid = "";
const char* password = "";
const char* ghost = "37.59.108.111";
const char* gport = "443";

unsigned char mac[6];
String clientMac = "";

bool PowerOnStatus = false;

bool GPS_Error = false;

DataStatus_t statusGPRS;

TaskHandle_t xTask_GPSUPDATE;

TickType_t xDelay1 = 1 / portTICK_PERIOD_MS;
TickType_t xDelay10 = 10 / portTICK_PERIOD_MS;
TickType_t xDelay100 = 100 / portTICK_PERIOD_MS;
TickType_t xDelay500 = 500 / portTICK_PERIOD_MS;
TickType_t xDelay1000 = 1000 / portTICK_PERIOD_MS;

void power_on( void * pvParameters );
String getGPS(char* ATcommand, char* expected_answer1, unsigned int timeout);
String SendDataToHTTP(String HTTPData1S, String host, String port);
int8_t sendATcommand(char* ATcommand, char* expected_answer1, unsigned int timeout);
int8_t sendATcommand2r(char* ATcommand, char* expected_answer1, char* expected_answer2, unsigned int timeout);
String macToStr(const uint8_t* mac);
void UpdatePositionTask( void * pvParameters );

/*################################################################################################################################################
##################################################################################################################################################
################################################################################################################################################*/

void setup() {  

  statusGPRS.GPS = 3;
  statusGPRS.GSM = 3;
  
  Serial.begin(115200);
  Serial2.begin(115200);

  Serial.println("Starting...");

  String taskMessage = "SETUP Task running on core ";
  taskMessage = taskMessage + xPortGetCoreID();
  Serial.println(taskMessage);

  SetupVibreur();
  InitSD();
  SetupLCD();
  SetupRTC();
  
  xTaskCreate(
                    power_on,   /* Function to implement the task */
                    "power_on", /* Name of the task */
                    10000,      /* Stack size in words */
                    NULL,       /* Task input parameter */
                    10,          /* Priority of the task */
                    NULL);  /* Core where the task should run */                   
}

void loop() {
  /*char buf[9];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", rtc_hour, rtc_minute, rtc_second);

  Serial.println(buf);
  vTaskDelay(xDelay1000);*/
}

/*################################################################################################################################################
##################################################################################################################################################
################################################################################################################################################*/

void power_on( void * pvParameters ){

  uint8_t gsmstatus = 3, gpsstatus = 3;
  
  String taskMessage = "PowerOn Task running on core ";
  taskMessage = taskMessage + xPortGetCoreID();
  Serial.println(taskMessage);
  
  uint8_t answer=0;
   
  // power on pulse
  pinMode(_rstpin, OUTPUT);

  digitalWrite(_rstpin, HIGH);
  vTaskDelay(xDelay10);
  digitalWrite(_rstpin, LOW);
  vTaskDelay(xDelay100);
  digitalWrite(_rstpin, HIGH);
  vTaskDelay(xDelay500);
    // waits for an answer from the module
  unsigned long previous = millis();
  while(answer == 0){    
    // Send AT every two seconds and wait for the answer
    answer = sendATcommand("AT", "OK", 2000);    
    if(!((millis() - previous) < 10000)){
      previous = millis();
      StartVibreur(500, 100, 2);
      digitalWrite(_rstpin, HIGH);
      vTaskDelay(xDelay10);
      digitalWrite(_rstpin, LOW);
      vTaskDelay(xDelay100);
      digitalWrite(_rstpin, HIGH);
      vTaskDelay(xDelay500);
    }
  }
  //statusGPRS.GPS = 3;
  statusGPRS.GSM = 2;
  if(uxQueueSpacesAvailable(StatusQueue) > 0){
    xQueueSend(StatusQueue, &statusGPRS, 0);
  }
  //tft.drawBitmap(16, 0, GSMICON, 16, 16, ILI9341_RED);
  uint8_t answer2 = 0, x = 0;
  char response[1024];
  previous = millis();
  memset(response, '\0', 1024);    // Initialize the string
  do{
    if(Serial2.available() != 0){ 
       
      response[x] = Serial2.read();
      x++;
      
      if (strstr(response, "+CPIN: SIM PIN") != NULL)    {        
        answer2 = 1; 
      }
    }else{
      vTaskDelay(xDelay1);
    }
        // Waits for the asnwer with time out
  }while((answer2 == 0) && ((millis() - previous) < 5000));
  

  sprintf(aux_string, "AT+CPIN=%s", pin);
  if(sendATcommand(aux_string, "OK", 2000) == 0){
    while(true){}
  }
  vTaskDelay(xDelay1000);
  Serial.println("Connecting to the network...");
  //tft.setTextSize(1);
  //tft.setCursor(0, 24);
  //tft.println("Connecting to the network...");
  //statusGPRS.GPS = 3;
  statusGPRS.GSM = 1;
  if(uxQueueSpacesAvailable(StatusQueue) > 0){
    xQueueSend(StatusQueue, &statusGPRS, 0);
  }
  while( (sendATcommand("AT+CREG?", "+CREG: 0,1", 500) || sendATcommand("AT+CREG?", "+CREG: 0,5", 500)) == 0 );

    
    snprintf(aux_string, sizeof(aux_string), "AT+CGSOCKCONT=1,\"IP\",\"%s\"", apn);
    sendATcommand(aux_string, "OK", 2000);
    sendATcommand("AT+CSOCKSETPN=1","OK",1000);
    //statusGPRS.GPS = 3;
    statusGPRS.GSM = 0;
    if(uxQueueSpacesAvailable(StatusQueue) > 0){
      xQueueSend(StatusQueue, &statusGPRS, 0);
    }
    //tft.drawBitmap(16, 0, GSMICON, 16, 16, ILI9341_GREEN);
    // set GPS server without certificate
    sendATcommand("AT+CGPSURL=\"supl.google.com:7276\"","OK",2000);
    sendATcommand("AT+CGPSSSL=0","OK",2000);
    statusGPRS.GPS = 2;
    //statusGPRS.GSM = 2;
    if(uxQueueSpacesAvailable(StatusQueue) > 0){
      xQueueSend(StatusQueue, &statusGPRS, 0);
    }
    //tft.drawBitmap(0, 0, GPSICON, 16, 16, ILI9341_RED);
    // start gps node in GPS-MS
    answer = sendATcommand("AT+CGPS=1,2","OK",2000);
    
    if (answer == 0){
        //tft.println("Error starting the GPS");
        //tft.println("Reboot");
        Serial.println("Error starting the GPS");
        Serial.println("Reboot");
        power_on(NULL);
        vTaskDelete( NULL );
    }
    while(sendATcommand("AT+CHTTPSSTART","OK",10000) == 0){
      sendATcommand("AT+CHTTPSSTOP","OK",10000);
    }

    getTime();
    
    WiFi.begin();
    WiFi.macAddress(mac);
    clientMac += macToStr(mac);

    Serial.println(clientMac);
    //tft.println(clientMac);

    StartVibreur(1000, 250, 2);
    xTaskCreatePinnedToCore(
                    UpdatePositionTask,   /* Function to implement the task */
                    "UpdatePositionTask", /* Name of the task */
                    10000,      /* Stack size in words */
                    NULL,       /* Task input parameter */
                    1,          /* Priority of the task */
                    &xTask_GPSUPDATE,
                    1);  /* Core where the task should run */
    
    vTaskDelete( NULL );
}

/*################################################################################################################################################
##################################################################################################################################################
################################################################################################################################################*/

void getTime(){

  StaticJsonBuffer<2048> jsonBuffer;

  String HTTPData = "GET ";
  HTTPData += "/time.php";
  HTTPData += " HTTP/1.1";

  String recvData = SendDataToHTTP(HTTPData, String(ghost), String(gport));
  char cdata[recvData.length()+1];
  recvData.toCharArray(cdata,recvData.length()+1);

  JsonObject& root = jsonBuffer.parseObject(recvData);
  
  const char* DtimeDayName = root["time"]["dayName"];
  int DtimeYear = root["time"]["year"];
  int DtimeMonth = root["time"]["month"];
  int DtimeDay = root["time"]["day"];
  int DtimeHours = root["time"]["hours"];
  int DtimeMinute = root["time"]["minute"];
  int DtimeSeconde = root["time"]["seconde"];

  DataTime_t DateTime; 
  
  DateTime.Dn = DtimeDayName;
  DateTime.Y = DtimeYear;
  DateTime.M = DtimeMonth;
  DateTime.D = DtimeDay;
  DateTime.h = DtimeHours;
  DateTime.m = DtimeMinute;
  DateTime.s = DtimeSeconde;

  if(uxQueueSpacesAvailable(SetTime) > 0){
    xQueueSend(SetTime, &DateTime, 0);
  }
}

/*################################################################################################################################################
##################################################################################################################################################
################################################################################################################################################*/

String getGPS(char* ATcommand, char* expected_answer1, unsigned int timeout){
  uint8_t x=0,  answer=0;
  char response[1024];
  unsigned long previous;

  memset(response, '\0', 1024);    // Initialize the string
    
  // Clean the input buffer
  while( Serial.available() > 0){ Serial.read();}    
  while( Serial2.available() > 0){ Serial2.read();}
  
  Serial2.println(ATcommand);    // Send the AT command 

  x = 0;
  previous = millis();

  // this loop waits for the answer
  do{
    if(Serial2.available() != 0){ 
       
      response[x] = Serial2.read();
      x++;
      // check if the desired answer is in the response of the module
      
      if (strstr(response, expected_answer1) != NULL)    {        
        answer = 1; 
      }
    }
        // Waits for the asnwer with time out
  }while((answer == 0) && ((millis() - previous) < timeout));    

  while(Serial2.available() > 0){
    response[x] = Serial2.read();
    x++;
  }
  
  char gpsdata[String(response).length()+1];
  String(response).toCharArray(gpsdata,String(response).length()+1);
  char *token;
  
  token = strtok(gpsdata, ":");
  
  char *latp = strtok(NULL, ",");
  char *latdir = strtok(NULL, ",");
  char *longp = strtok(NULL, ",");
  char *longdir = strtok(NULL, ",");
  char *date = strtok(NULL, ",");
  char *timec = strtok(NULL, ",");
  char *altitude = strtok(NULL, ",");
  char *speed_kph = strtok(NULL, ",");
  char *heading = strtok(NULL, "\r\nAmpI/AmpQ");
  
  if(latp != NULL && longp != NULL){

    double latitude = atof(latp);
    double longitude = atof(longp);
    
    // convert latitude from minutes to decimal
    double deg = floor(latitude / 100);
    double minutes = latitude - (100 * deg);
    minutes /= 60;
    deg += minutes;

    // turn direction into + or -
    if (latdir[0] == 'S') deg *= -1;

    double lat = deg;

    // convert longitude from minutes to decimal
    deg = floor(longitude / 100);
    minutes = longitude - (100 * deg);
    minutes /= 60;
    deg += minutes;

    // turn direction into + or -
    if (longdir[0] == 'W') deg *= -1;

    double lon = deg;
    
    char latc[32];
    char lonc[32];
    
    sprintf(latc, "%f", lat*1000);
    sprintf(lonc, "%f", lon*1000);

    String dataSend = String(latc);
    dataSend += "|";
    dataSend += String(lonc);
    dataSend += "|";
    dataSend += String(date);
    dataSend += "|";
    dataSend += String(timec);
    dataSend += "|";
    dataSend += String(altitude);
    dataSend += "|";
    dataSend += String(speed_kph);
    dataSend += "|";
    dataSend += String(heading);
    dataSend += "|";

    return dataSend;
  }else{
    return "ERROR";
  }
}

/*################################################################################################################################################
##################################################################################################################################################
################################################################################################################################################*/

String SendDataToHTTP(String HTTPData1S, String host, String port){

  String HTTPRequestS = "AT+CHTTPSOPSE=\"";
  HTTPRequestS += String(host);
  HTTPRequestS += "\", ";
  HTTPRequestS += String(port);
  HTTPRequestS += ",2";
  
  String HTTPData2S = "Host: ";
  HTTPData2S += String(host);
  HTTPData2S += ":";
  HTTPData2S += String(port);
    
  char HTTPRequest[HTTPRequestS.length()+1];
  HTTPRequestS.toCharArray(HTTPRequest,HTTPRequestS.length()+1);

  if(sendATcommand(HTTPRequest,"OK",10000) == 1){
    String HTTPStartRequestS = "AT+CHTTPSSEND="+String(HTTPData1S.length()+HTTPData2S.length()+6);
    char HTTPStartRequest[HTTPStartRequestS.length()+1];
    HTTPStartRequestS.toCharArray(HTTPStartRequest,HTTPStartRequestS.length()+1);
    
    sendATcommand(HTTPStartRequest,">",1000);
    //Serial2.print(HTTPData1S);
    //Serial2.print("\r\n");
    //Serial2.print(HTTPData2S);
    //Serial2.print("\r\n");
    //Serial2.print("\r\n");
    
    uint8_t answer = 0, x = 0;
    char response[1024];
    unsigned long previous;
    
    memset(response, '\0', 1024);    // Initialize the string

    vTaskDelay(xDelay100);
  
    // Clean the input buffer
    while( Serial.available() > 0){ Serial.read();}    
    while( Serial2.available() > 0){ Serial2.read();}

    Serial2.print(HTTPData1S);
    Serial2.print("\r\n");
    Serial2.print(HTTPData2S);
    Serial2.print("\r\n");
    Serial2.print("\r\n");
    
    x = 0;
    previous = millis();
    do{
      if(Serial2.available() != 0){  
        response[x] = Serial2.read();
        x++;
        if (strstr(response, "+CHTTPS: RECV EVENT") != NULL || strstr(response, "+CHTTPSNOTIFY: PEER CLOSED") != NULL )    {        
          answer = 1; 
        }
      }
    }while((answer == 0) && ((millis() - previous) < 100000));    
    
    answer = 0;
    x = 0;
    previous = millis();
    memset(response, '\0', 1024);    // Initialize the string
    
    do{
      if(Serial2.available() != 0){  
        response[x] = Serial2.read();
        x++;
        if (strstr(response, "+CHTTPS: RECV EVENT") != NULL || strstr(response, "+CHTTPSNOTIFY: PEER CLOSED") != NULL )    {        
          answer = 1; 
        }
      }
    }while((answer == 0) && ((millis() - previous) < 10000));  
    
    //Serial2.println("AT+CHTTPSRECV?");
    
    answer = 0;
    x = 0;
    previous = millis();
    memset(response, '\0', 1024);    // Initialize the string

    vTaskDelay(xDelay100);
  
    // Clean the input buffer
    while( Serial.available() > 0){ Serial.read();}    
    while( Serial2.available() > 0){ Serial2.read();}
    
    Serial2.println("AT+CHTTPSRECV?");
    do{
    if(Serial2.available() != 0){ 
      response[x] = Serial2.read();
      x++;
      // check if the desired answer is in the response of the module
      
      if (strstr(response, "+CHTTPSRECV: LEN,") != NULL)    {        
        answer = 1; 
      }
    }
        // Waits for the asnwer with time out
    }while((answer == 0) && ((millis() - previous) < 10000));    

    while(Serial2.available() > 0){
      response[x] = Serial2.read();
      x++;
    }
    char httpdatalen[String(response).length()+1];
    String(response).toCharArray(httpdatalen,String(response).length()+1);
    char *token;
  
    token = strtok(httpdatalen, ",");
  
    char *datalen = strtok(NULL, "\r\n");
    
    String GetHTTPDataS = "AT+CHTTPSRECV=";
    GetHTTPDataS += String(datalen);
    
    char GetHTTPData[String(GetHTTPDataS).length()+1];
    GetHTTPDataS.toCharArray(GetHTTPData,GetHTTPDataS.length()+1);
    
    //Serial2.println(GetHTTPData);

    unsigned int datarflen = String(datalen).toInt()*10;
    char responsef[datarflen];
    answer = 0;
    x = 0;
    previous = millis();
    memset(responsef, '\0', datarflen);    // Initialize the string

    vTaskDelay(xDelay100);
  
    // Clean the input buffer
    while( Serial.available() > 0){ Serial.read();}    
    while( Serial2.available() > 0){ Serial2.read();}
    
    Serial2.println(GetHTTPData);
    String GetHTTPDataTemp;
    do{
      if(Serial2.available() != 0){ 

        GetHTTPDataTemp = Serial2.readString();
      }else{
        GetHTTPDataTemp.toCharArray(responsef,GetHTTPDataTemp.length()+1);
      }
        
        // check if the desired answer is in the response of the module
        if (strstr(responsef, "|DATA_STOP|") != NULL)    {        
          answer = 1; 
        }
      
        // Waits for the asnwer with time out
    }while((answer == 0) && ((millis() - previous) < 10000)); 
    sendATcommand2r("AT+CHTTPSCLSE","OK","ERROR",10000);
    String responsefs = String(responsef);
    int index = responsefs.indexOf("DATA_START|");
    String TempData1 = responsefs.substring(index-1);
    TempData1.replace("|DATA_START|", "");
    TempData1.replace("|DATA_STOP|", "$");
    return TempData1.substring(0, TempData1.lastIndexOf('$'));
    
  }else{
    sendATcommand2r("AT+CHTTPSCLSE","OK","ERROR",10000);
    return "HTTPERROR";
  }
}

/*################################################################################################################################################
##################################################################################################################################################
################################################################################################################################################*/

int8_t sendATcommand(char* ATcommand, char* expected_answer1, unsigned int timeout){
  uint8_t x=0,  answer=0;
  char response[1024];
  unsigned long previous;

  memset(response, '\0', 1024);    // Initialize the string

  vTaskDelay(xDelay100);
  
  // Clean the input buffer
  while( Serial.available() > 0){ Serial.read();}    
  while( Serial2.available() > 0){ Serial2.read();}
  
  Serial2.println(ATcommand);    // Send the AT command 
  //Serial.print("Commande: ");
  //Serial.println(ATcommand);
  x = 0;
  previous = millis();

  // this loop waits for the answer
  do{
    if(Serial2.available() != 0){ 
       
      response[x] = Serial2.read();
      x++;
      // check if the desired answer is in the response of the module
      
      if (strstr(response, expected_answer1) != NULL)    {        
        answer = 1; 
      }
    }else{
      vTaskDelay(xDelay1);
    }
        // Waits for the asnwer with time out
  }while((answer == 0) && ((millis() - previous) < timeout));    

  while(Serial2.available() > 0){
    response[x] = Serial2.read();
    x++;
  }
  //Serial.print("Data: ");
  //Serial.println(response);
  return answer;
   
}

/*################################################################################################################################################
##################################################################################################################################################
################################################################################################################################################*/

int8_t sendATcommand2r(char* ATcommand, char* expected_answer1, char* expected_answer2, unsigned int timeout){
  uint8_t x=0,  answer=0;
  char response[1024];
  unsigned long previous;

  memset(response, '\0', 1024);    // Initialize the string
  
  vTaskDelay(xDelay100);
   
  // Clean the input buffer
  while( Serial.available() > 0){ Serial.read();}    
  while( Serial2.available() > 0){ Serial2.read();}
  
  Serial2.println(ATcommand);    // Send the AT command 
  //Serial.print("Commande: ");
  //Serial.println(ATcommand);
  x = 0;
  previous = millis();

  // this loop waits for the answer
  do{
    if(Serial2.available() != 0){ 
       
      response[x] = Serial2.read();
      x++;
      // check if the desired answer is in the response of the module
      
      if (strstr(response, expected_answer1) != NULL)    {        
        answer = 1; 
      }
      if (strstr(response, expected_answer2) != NULL)    {        
        answer = 2; 
      }
    }else{
      vTaskDelay(xDelay1);
    }
        // Waits for the asnwer with time out
  }while((answer == 0) && ((millis() - previous) < timeout));    

  while(Serial2.available() > 0){
    response[x] = Serial2.read();
    x++;
  }
  //Serial.print("Data: ");
  //Serial.println(response);
  return answer;
   
}

/*################################################################################################################################################
##################################################################################################################################################
################################################################################################################################################*/

void UpdatePositionTask( void * pvParameters ){
  String taskMessage = "UpdatePosition Task running on core ";
  taskMessage = taskMessage + xPortGetCoreID();
  Serial.println(taskMessage);
  
  while(true){
    String TempGPSData = getGPS("AT+CGPSINFO","+CGPSINFO:",1000);
    char data[TempGPSData.length()+1];
    TempGPSData.toCharArray(data,TempGPSData.length()+1);
    //Serial.println(TempGPSData);
    if(TempGPSData != "ERROR"){

      char *lat = strtok(data, "|");
      char *lon = strtok(NULL, "|");
  
      String HTTPData = "GET ";
      HTTPData += "/ajax/addgpsloc.php?Latitude=";
      HTTPData += String(lat);
      HTTPData += "&Longitude=";
      HTTPData += String(lon);
      HTTPData += "&Mac=";
      HTTPData += clientMac;
      HTTPData += " HTTP/1.1";

      String recvData = SendDataToHTTP(HTTPData, String(ghost), String(gport));
      char recvDataChar[recvData.length()+1];
      recvData.toCharArray(recvDataChar,recvData.length()+1);
      /*tft.fillScreen(ILI9341_BLACK);
      tft.setCursor(0, 0);
      tft.setTextColor(ILI9341_GREEN);
      tft.setTextSize(1);
      tft.println(TempGPSData);
      tft.println("");
      tft.setTextColor(ILI9341_WHITE);
      tft.setTextSize(1);
      tft.println(recvData);*/   
      Serial.println(TempGPSData);
      Serial.print("Data: ");  
      Serial.println(recvData);

      tftfillRect(0,17, (TempGPSData.length())*6,8,ILI9341_BLACK);
      tftfillRect(0,25, (recvData.length())*12,16,ILI9341_BLACK);
      tftPrintLn(data, (int16_t)0, (int16_t)17, (uint8_t)1, (uint16_t)ILI9341_GREEN);
      tftPrintLn(recvDataChar, (uint8_t)2, (uint16_t)ILI9341_RED);
      GPS_Error = false;
      //tft.drawBitmap(0, 0, GPSICON, 16, 16, ILI9341_GREEN);
      statusGPRS.GPS = 0;
      //statusGPRS.GSM = 2;
      if(uxQueueSpacesAvailable(StatusQueue) > 0){
        xQueueSend(StatusQueue, &statusGPRS, 0);
      }
    }else{
      if(GPS_Error == false){
        Serial.println(TempGPSData);
        Serial.print("Data: ");
        Serial.println("NO GPS!");
        /*tft.fillScreen(ILI9341_BLACK);
        tft.setCursor(0, 0);
        tft.setTextColor(ILI9341_GREEN);
        tft.setTextSize(1);
        tft.println(TempGPSData);
        tft.println("");
        tft.setTextColor(ILI9341_RED);
        tft.setTextSize(2);
        tft.println("NO GPS!");*/
        //tft.drawBitmap(0, 0, GPSICON, 16, 16, ILI9341_YELLOW);
        statusGPRS.GPS = 1;
        //statusGPRS.GSM = 2;
        if(uxQueueSpacesAvailable(StatusQueue) > 0){
          xQueueSend(StatusQueue, &statusGPRS, 0);
        }
        tftfillRect(0,17, (TempGPSData.length())*6,8,ILI9341_BLACK);
        tftfillRect(0,25, 84,16,ILI9341_BLACK);
        tftPrintLn(data, (int16_t)0, (int16_t)17, (uint8_t)1, (uint16_t)ILI9341_GREEN);
        tftPrintLn("NO GPS!", (uint8_t)2, (uint16_t)ILI9341_RED);
        GPS_Error = true;
      }
    }
  }
  vTaskDelete( NULL );
}
