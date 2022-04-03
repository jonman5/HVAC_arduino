/* HVACinator project for COEN/ELEC 390
Arduino sketch to control sensors and vent
*/

#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <time.h>
#include <ctime>
#include <Firebase_ESP_Client.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

#include <DHT.h>
#include "Servo.h"
#include <Wire.h>
#include <Adafruit_MLX90614.h>

#define DHT11PIN 14
#define DHTTYPE DHT11

DHT dht(DHT11PIN, DHTTYPE);
float h,t_room,tf;
double t_vent;

 
Servo myservo;
int angle = 0;  
int x=0;


Adafruit_MLX90614 mlx = Adafruit_MLX90614();

//WIFI Network credentials
#define WIFI_SSID "18XPJ"
#define WIFI_PASSWORD "COEN390TEST"

//Firebase API Key
#define API_KEY "AIzaSyDvVu90GPznMnkwIJxbl4wnNF6lrfXI8pw"

//RTDB URL
#define DATABASE_URL "https://hvacinator-default-rtdb.firebaseio.com/" 

//Set global firebase variables for connecting and storing data
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

//Set unitID
String unitID = "111111";


unsigned long sendDataPrevMillis = 0;
int count = 0;

bool storeCurrentSensorData(float temperature, float humidity, double ventTemp);
bool storeLogs(float temperature, float humidity, double ventTemp, String time);

void setup(){
  delay(200);
  Serial.begin(115200);
  
  //Initialize sensors
  myservo.attach(13);
  dht.begin();
  mlx.begin();
  delay(1000);
  Serial.println("DHT11 Temperature and Humidity ");
  
  //Connect to WIFI
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print("...");
    delay(1000);
  }
  Serial.println();
  Serial.println("Connection to WIFI established!IP: ");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  //Connect to time server to retrieve current time
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Connecting to time server");
  while (!time(nullptr)) {
    Serial.print("...");
    delay(1000);
  }
  Serial.println();

  //Set the timezone
  setenv("TZ","EST5EDT,M3.2.0,M11.1.0",1);
  tzset();

  //Set the database API key
  config.api_key = API_KEY;

  //Set the database URL
  config.database_url = DATABASE_URL;

  // Assign the callback function for the long running token generation task
  config.token_status_callback = tokenStatusCallback;
  
  //Set databse username and password for connection
  //Connect to database
  auth.user.email = "joe@gmail.com";
  auth.user.password = "hvacinator123";
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop(){
  if (Firebase.ready() && (millis() - sendDataPrevMillis > 2000 || sendDataPrevMillis == 0)){
    //Reset timer
    sendDataPrevMillis = millis();
    x++;

    //Read sensor data
    h = dht.readHumidity();
    delay(100);
    t_room = dht.readTemperature();
    delay(100); 
    t_vent = mlx.readObjectTempC();
    t_vent = mlx.readObjectTempC();
    delay(100);

    //Print sensor data to serial console for debugging
    Serial.print('\n');
    Serial.print("Humidity in room = ");
    Serial.print(h);
    Serial.print("%,  ");
    Serial.print("Temperature in room = ");
    Serial.print(t_room);
    Serial.println("°C, ");
    Serial.print("Sensor2 Temp= "); Serial.print(t_vent);
    Serial.println("°C");
    Serial.print('\n');
    
    //Set servo angle
    myservo.write(90);
    
    //Get current time from time server
    time_t now = time(nullptr);
    tm* time_local = localtime(&now);

    //Format current time and save in string
    char* time_string = new char [40];
    strftime(time_string, 40, "%F %X", time_local); //time_string is formatted at YYYY-MM-DD HH:mm:SS
    Serial.println(time_string);
    
    // Write current sensor data to real time database
    if (storeCurrentSensorData(t_room, h, t_vent)){
      Serial.println("Current sensor data updated in RTDB for " + unitID);
      
      // Write current sensor data to logs in real time database
      if (storeLogs(t_room, h, t_vent, time_string)){
        Serial.print("Sensor logs stored in RTDB for ");
        Serial.println(time_string);
      }
      else {
        Serial.println("Log data entry failed");
      }
    }
  } 
}

bool storeCurrentSensorData(float temperature, float humidity, double ventTemp){
    // Write current sensor data to real time database under units/unitID
    if (Firebase.RTDB.setFloat(&fbdo, "units/" + unitID + "/humidity", h)){
      // Serial.println("PASSED");
      // Serial.println("PATH: " + fbdo.dataPath());
      // Serial.println("TYPE: " + fbdo.dataType());
    }
    else {
      Serial.println("FAILED");
      Serial.println("Error: " + fbdo.errorReason());
      return false;
    }

    if (Firebase.RTDB.setFloat(&fbdo, "units/" + unitID + "/temperature", t_room)){
      // Serial.println("PASSED");
      // Serial.println("PATH: " + fbdo.dataPath());
      // Serial.println("TYPE: " + fbdo.dataType());
    }
    else {
      Serial.println("FAILED");
      Serial.println("Error: " + fbdo.errorReason());
      return false;
    }  
    
    if (Firebase.RTDB.setDouble(&fbdo, "units/" + unitID + "/temperatureVent", t_vent)){
      // Serial.println("PASSED");
      // Serial.println("PATH: " + fbdo.dataPath());
      // Serial.println("TYPE: " + fbdo.dataType());
    }
    else {
      Serial.println("FAILED");
      Serial.println("Error: " + fbdo.errorReason());
      return false;
    }
    return true;
}

bool storeLogs(float temperature, float humidity, double ventTemp, String time){
    // Add current sensor data to logs under units/unitID/logs/time
  if (Firebase.RTDB.setFloat(&fbdo, "units/" + unitID + "/logs/" + time + "/humidity", h)){
    // Serial.println("PASSED");
    // Serial.println("PATH: " + fbdo.dataPath());
    // Serial.println("TYPE: " + fbdo.dataType());
  }
  else {
    Serial.println("FAILED");
    Serial.println("Error: " + fbdo.errorReason());
    return false;
  }

  if (Firebase.RTDB.setFloat(&fbdo, "units/" + unitID + "/logs/" + time + "/temperature", t_room)){
    // Serial.println("PASSED");
    // Serial.println("PATH: " + fbdo.dataPath());
    // Serial.println("TYPE: " + fbdo.dataType());
  }
  else {
    Serial.println("FAILED");
    Serial.println("Error: " + fbdo.errorReason());
    return false;
  }  
  
  if (Firebase.RTDB.setDouble(&fbdo, "units/" + unitID + "/logs/" + time + "/temperatureVent", t_vent)){
    // Serial.println("PASSED");
    // Serial.println("PATH: " + fbdo.dataPath());
    // Serial.println("TYPE: " + fbdo.dataType());
  }
  else {
    Serial.println("FAILED");
    Serial.println("Error: " + fbdo.errorReason());
    return false;
  }
  return true;
}
