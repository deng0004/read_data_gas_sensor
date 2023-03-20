/*
  MQUnifiedsensor Library - reading an MQ2

  Demonstrates the use a MQ2 sensor.
  Library originally added 01 may 2019
  by Miguel A Califa, Yersson Carrillo, Ghiordy Contreras, Mario Rodriguez
 
  Added example
  modified 23 May 2019
  by Miguel Califa 

  Updated library usage
  modified 26 March 2020
  by Miguel Califa 
  Wiring:
  https://github.com/miguel5612/MQSensorsLib_Docs/blob/master/static/img/MQ_Arduino.PNG
  Please make sure arduino A0 pin represents the analog input configured on #define pin

 This example code is in the public domain.

*/

//Include the library
#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#include <FirebaseESP32.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#elif defined(ARDUINO_RASPBERRY_PI_PICO_W)
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <FirebaseESP8266.h>
#endif
#include <MQUnifiedsensor.h>
#include "time.h"

/************************Hardware Related Macros************************************/
#define         Board                   ("ESP32")
#define         Pin                     (33)  //Analog input 3 of your arduino
/***********************Software Related Macros************************************/
#define         Type                    ("MQ-2") //MQ2
#define         Voltage_Resolution      (3.3)
#define         ADC_Bit_Resolution      (12) // For arduino UNO/MEGA/NANO
#define         RatioMQ2CleanAir        (9.83) //RS / R0 = 9.83 ppm 


//Provide the token generation process info.
#include <addons/TokenHelper.h>

//Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

#define API_KEY "api_link"
#define DATABASE_URL "database_link"        

/* 1. Define the WiFi credentials */
#define WIFI_SSID "ssid"                                  
#define WIFI_PASSWORD "password" 

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
int timestamp;
const char* time_zone = "CET-1CEST,M3.5.0,M10.5.0/3";  // TimeZone rule for Europe/Rome including daylight adjustment rules (optional)

//Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

/*****************************Globals***********************************************/
MQUnifiedsensor MQ2(Board, Voltage_Resolution, ADC_Bit_Resolution, Pin, Type);
/*****************************Globals***********************************************/

// Database main path (to be updated in setup with the user UID)
String databasePath;
// Database child nodes
// String presPath = "/pressure";
String mq2Path = "/mq2";
String timePath = "/timestamp";

// Parent Node (to be updated in every loop)
String parentPath;

FirebaseJson json;

const char* ntpServer = "pool.ntp.org";

// Timer variables (send new readings every three minutes)
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 60000;


// Initialize WiFi
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}

// Function that gets current epoch time
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}



void setup() {
  //Init the serial port communication - to debug the library
  Serial.begin(115200); //Init serial port

 
  //Set math model to calculate the PPM concentration and the value of constants
  MQ2.setRegressionMethod(1); //_PPM =  a*ratio^b
  MQ2.setA(36974); MQ2.setB(-3.109); // Configure the equation to to calculate LPG concentration
  /*
    Exponential regression:
    Gas    | a      | b
    H2     | 987.99 | -2.162
    LPG    | 574.25 | -2.222
    CO     | 36974  | -3.109
    Alcohol| 3616.1 | -2.675
    Propane| 658.71 | -2.168
  */
  
  // delay(2000);
  // WiFi.begin(WIFI_SSID, WIFI_PASSWORD);                                  
  // Serial.print("Connecting to ");
  // Serial.print(WIFI_SSID);
  // while (WiFi.status() != WL_CONNECTED) {
  //   Serial.print(".");
  //   delay(500);
  // }
  initWiFi();
  configTime(0, 0, ntpServer);
  Firebase.begin(DATABASE_URL, API_KEY);  






  /*****************************  MQ Init ********************************************/ 
  //Remarks: Configure the pin of arduino as input.
  /************************************************************************************/ 
  MQ2.init(); 
  MQ2.setRL(4.7);
  /* 
    //If the RL value is different from 10K please assign your RL value with the following method:
    MQ2.setRL(10);
  */
  /*****************************  MQ CAlibration ********************************************/ 
  // Explanation: 
   // In this routine the sensor will measure the resistance of the sensor supposedly before being pre-heated
  // and on clean air (Calibration conditions), setting up R0 value.
  // We recomend executing this routine only on setup in laboratory conditions.
  // This routine does not need to be executed on each restart, you can load your R0 value from eeprom.
  // Acknowledgements: https://jayconsystems.com/blog/understanding-a-gas-sensor
  Serial.print("Calibrating please wait.");
  float calcR0 = 0;
  for(int i = 1; i<=10; i ++)
  {
    MQ2.update(); // Update data, the arduino will read the voltage from the analog pin
    calcR0 += MQ2.calibrate(RatioMQ2CleanAir);
    Serial.print(".");
  }
  MQ2.setR0(calcR0/10);
  Serial.println("  done!.");
  
  if(isinf(calcR0)) {Serial.println("Warning: Conection issue, R0 is infinite (Open circuit detected) please check your wiring and supply"); while(1);}
  if(calcR0 == 0){Serial.println("Warning: Conection issue found, R0 is zero (Analog pin shorts to ground) please check your wiring and supply"); while(1);}
  /*****************************  MQ CAlibration ********************************************/ 

  // MQ2.serialDebug(true);

    /**
   * This will set configured ntp servers and constant TimeZone/daylightOffset
   * should be OK if your time zone does not need to adjust daylightOffset twice a year,
   * in such a case time adjustment won't be handled automagicaly.
   */
  // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);


  // Serial.println();
  // Serial.print("Connected");
  // Serial.print("IP Address: ");
  // Serial.println(WiFi.localIP());                               //prints local IP address
  // Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  // /* Assign the api key (required) */
  // config.api_key = API_KEY;

  // config.database_url = DATABASE_URL;

  //////////////////////////////////////////////////////////////////////////////////////////////
  //Please make sure the device free Heap is not lower than 80 k for ESP32 and 10 k for ESP8266,
  //otherwise the SSL connection will fail.
  //////////////////////////////////////////////////////////////////////////////////////////////

  // Firebase.begin(DATABASE_URL, API_KEY);

  //Comment or pass false value when WiFi reconnection will control by your code or third party library
//  Firebase.reconnectWiFi(true);

//   Firebase.setDoubleDigits(5);

}

char *database_link(char chemical_name[20])
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
  }


  Serial.println();
  char year[5];
  strftime(year, 5, "%Y", &timeinfo);
  Serial.println(year);
  Serial.println();

  char month[3];
  strftime(month, 3, "%m", &timeinfo);
  Serial.println(month);
  Serial.println();

  char day[3];
  strftime(day, 3, "%d", &timeinfo);
  Serial.println(day);
  Serial.println();

  char hours[3];
  strftime(hours, 3, "%H", &timeinfo);
  Serial.println(hours);
  Serial.println();

  char minutes[3];
  strftime(minutes, 3, "%M", &timeinfo);
  Serial.println(minutes);
  Serial.println();

  char seconds[3];
  strftime(seconds, 3, "%S", &timeinfo);
  Serial.println(seconds);
  Serial.println();   
  

  char settings_data[100];
  const char device_id[4] = "001";
  strcpy(settings_data, device_id);
  strcat(settings_data, chemical_name);
  strcat(settings_data, year);
  strcat(settings_data, month);
  strcat(settings_data, day);
  strcat(settings_data, "/");
  strcat(settings_data, hours);
  strcat(settings_data, minutes);
  strcat(settings_data, seconds);   
  Serial.print(settings_data);
  
  return settings_data;
}



void loop() {
  // MQ2.update(); // Update data, the arduino will read the voltage from the analog pin
//   float data_ppm = MQ2.readSensor(); // Sensor will read PPM concentration using the model, a and b values set previously or from the setup
// //  MQ2.serialDebug(); // Will print the table on the serial port
//   Serial.print("PPM: ");  
//   Serial.print(data_ppm);
//   Serial.println("");
//   delay(2000);

 if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0))                                     // Checking sensor working
 {   
  sendDataPrevMillis = millis();

  

  //Get current timestamp
  timestamp = getTime();
  Serial.print ("time: ");
  Serial.println (timestamp);

  parentPath=  "/" + String(timestamp);
  // char* d;
  // d = database_link("CO");
  // Serial.print(d);

  MQ2.update();
  json.set(mq2Path.c_str(), String(MQ2.readSensor()));
  json.set(timePath, String(timestamp));
  Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());  


  //Firebase.setInt(fbdo, main, 5);
  // Firebase.setFloat(fbdo, "/test/b", data_ppm);
  // Firebase.setFloat(fbdo, d, data_ppm);
  // Firebase.setFloat(fbdo, "/test/b", y);
  // delay(200);                                
  
 }
}
