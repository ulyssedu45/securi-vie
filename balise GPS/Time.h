#include <stdio.h>
#include <DS1302.h>

const int kCePin   = 27;  // Chip Enable
const int kIoPin   = 14;  // Input/Output
const int kSclkPin = 12;  // Serial Clock

String rtc_day;
int rtc_year;
int rtc_month;
int rtc_date;
int rtc_hour;
int rtc_minute;
int rtc_second;

QueueHandle_t SetTime;

DS1302 rtc(kCePin, kIoPin, kSclkPin);

void UpdateTime( void * pvParameters );
String dayAsString(const Time::Day day);
void SetupRTC();

typedef struct DataTime_t{
  int Y;
  int M;
  int D;
  int h;
  int m;
  int s;
  const char* Dn;
};

String dayAsString(const Time::Day day) {
  switch (day) {
    case Time::kSunday: return "Dimanche";
    case Time::kMonday: return "Lundi";
    case Time::kTuesday: return "Mardi";
    case Time::kWednesday: return "Mercredi";
    case Time::kThursday: return "Jeudi";
    case Time::kFriday: return "Vendredi";
    case Time::kSaturday: return "Samedi";
  }
  return "(unknown day)";
}

Time::Day stringAsDay(const String day) {
  if(day == "Sunday"){
    return Time::kSunday;
  }else if(day == "Monday"){
    return Time::kMonday;
  }else if(day == "Tuesday"){
    return Time::kTuesday;
  }else if(day == "Wednesday"){
    return Time::kWednesday;
  }else if(day == "Thursday"){
    return Time::kThursday;
  }else if(day == "Friday"){
    return Time::kFriday;
  }else if(day == "Saturday"){
    return Time::kSaturday;
  }else{
    return Time::kSaturday;
  }
}

DataTime_t DateTime;

void UpdateTime( void * pvParameters ) {
  TickType_t xDelay = 1000 / portTICK_PERIOD_MS;

  String taskMessage = "updateTime Task running on core ";
  taskMessage = taskMessage + xPortGetCoreID();
  Serial.println(taskMessage);
  
  while(true){

    if(uxQueueMessagesWaiting(SetTime) != 0){
      xQueueReceive(SetTime, &DateTime, 0);

      Time::Day DateName = stringAsDay(DateTime.Dn);

      Time t(DateTime.Y, DateTime.M, DateTime.D, DateTime.h, DateTime.m, DateTime.s, DateName);

      // Set the time and date on the chip.
      rtc.time(t);
    }
    
    Time t = rtc.time();
    
    rtc_day = dayAsString(t.day);
    rtc_year = t.yr;
    rtc_month = t.mon;
    rtc_date = t.date;
    rtc_hour = t.hr;
    rtc_minute = t.min;
    rtc_second = t.sec;
    
    vTaskDelay(xDelay);
    //delay(1000);
  }
  vTaskDelete( NULL );
}

void SetupRTC(){
  rtc.writeProtect(false);
  rtc.halt(false);
  
  SetTime = xQueueCreate(1, sizeof(struct DataTime_t) );
  
  // Make a new time object to set the date and time.
  // Sunday, September 22, 2013 at 01:38:50.
  //Time t(2018, 4, 16, 18, 29, 30, Time::kMonday);

  // Set the time and date on the chip.
  //rtc.time(t);
  
  xTaskCreate(
                    UpdateTime,   /* Function to implement the task */
                    "UpdateTime", /* Name of the task */
                    10000,      /* Stack size in words */
                    NULL,       /* Task input parameter */
                    20,          /* Priority of the task */
                    NULL);  /* Core where the task should run */
}
