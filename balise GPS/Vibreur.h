#include "esp_task_wdt.h"

QueueHandle_t VibreurQueue;

const int _vibreurpin = 15;

typedef struct VibreurData {
    int onTime;
    int offTime;
    int repetition;
};

void TaskVibreur( void * parameter );
void StartVibreur(int onTime, int offTime, int repetition);
void SetupVibreur();

void SetupVibreur(){
  pinMode(_vibreurpin, OUTPUT);

  VibreurQueue = xQueueCreate( 20, sizeof( struct VibreurData ) );

  xTaskCreate(
                    TaskVibreur,   /* Function to implement the task */
                    "TaskVibreur", /* Name of the task */
                    10000,      /* Stack size in words */
                    NULL,       /* Task input parameter */
                    40,          /* Priority of the task */
                    NULL);  /* Core where the task should run */
                    
  StartVibreur(1000, 250, 1);
}

void TaskVibreur( void * parameter ){
    String taskMessage = "Vibreur Task running on core ";
    taskMessage = taskMessage + xPortGetCoreID();
    Serial.println(taskMessage);
    
    TickType_t xDelay = 100 / portTICK_PERIOD_MS;
  
    struct VibreurData Data;

    TickType_t xOnTime;
    TickType_t xOffTime;
    int repetition;
    
    while(true){
      while(uxQueueMessagesWaiting(VibreurQueue) > 0){
        xQueueReceive(VibreurQueue, &Data, 0);

        xOnTime = Data.onTime / portTICK_PERIOD_MS;
        xOffTime = Data.offTime / portTICK_PERIOD_MS;
        repetition = Data.repetition;
        
        while(repetition > 0){
          digitalWrite(_vibreurpin, HIGH);
          vTaskDelay(xOnTime);
          digitalWrite(_vibreurpin, LOW);
          if(repetition != 1 || uxQueueMessagesWaiting(VibreurQueue) != 0){
            vTaskDelay(xOffTime);
          }
          repetition--;
        }
      }  
      vTaskDelay(xDelay);
    }
    vTaskDelete( NULL );
}

void StartVibreur(int onTime, int offTime, int repetition){
  struct VibreurData data;

  data.onTime = onTime;
  data.offTime = offTime;
  data.repetition = repetition;

  //xQueueOverwrite( VibreurQueue, &data );
  if(uxQueueSpacesAvailable(VibreurQueue) > 0){
    xQueueSend(VibreurQueue, &data, 0);
  }
}
