/* HVACinator project for COEN/ELEC 390
Created by Team 7
Arduino sketch to control sensors and vent

This sketch uses the Firebase-ESP-Client library created by K. Suwatchai (Mobizt)
https://github.com/mobizt/Firebase-ESP-Client
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
#include "addons/TokenHelper.h"
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

Adafruit_MLX90614 mlx = Adafruit_MLX90614();

//WIFI Network credentials
#define WIFI_SSID "18XPJ"
#define WIFI_PASSWORD "COEN390TEST"

//Firebase API Key
#define API_KEY "AIzaSyDvVu90GPznMnkwIJxbl4wnNF6lrfXI8pw"

//RTDB URL
#define DATABASE_URL "https://hvacinator-default-rtdb.firebaseio.com/" 
//Firebase project ID
#define FIREBASE_PROJECT_ID "hvacinator"

//Set global firebase variables for connecting and storing data
FirebaseData fbdo;
FirebaseData fbdo2;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseJson JSONdata;
FirebaseJsonData result;

//Set unitID
String unitID = "111111";


unsigned long sendDataPrevMillis = 0;
int count = 0;
double currentTargetTemp = 0;
int currentVentPosition = 0;

bool storeCurrentSensorData(float temperature, float humidity, double ventTemp);
bool storeLogs(float temperature, float humidity, double ventTemp, String time);
double getSetTemp();
void setVentOpening(int percent);
int control_function(float room_temp, float vent_temp, float target_temp, int prev_state);

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

    //Read sensor data
    h = dht.readHumidity();
    delay(50);
    t_room = dht.readTemperature();
    delay(50); 
    t_vent = mlx.readAmbientTempC();
    t_vent = mlx.readAmbientTempC();

    //Print sensor data to serial console for debugging
    Serial.print('\n');
    Serial.print("Humidity in room = ");
    Serial.print(h);
    Serial.print("%,  ");
    Serial.print("Temperature in room = ");
    Serial.print(t_room);
    Serial.println("°C, ");
    Serial.print("Sensor2 Temp= "); Serial.print(t_vent);
    Serial.println("°C\n");
    
    double newTarget = getTargetTemp();
    if (newTarget != -9999){
      currentTargetTemp = newTarget;
    }
    Serial.print("\nTarget Temp: ");
    Serial.println(currentTargetTemp);
 
    
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
      delay(100);
      if (storeLogs(t_room, h, t_vent, time_string)){
        Serial.print("Sensor logs stored in RTDB for ");
        Serial.println(time_string);
      }
      else {
        Serial.println("Log data entry failed");
      }
    }
    int newVentPosition = control_function(t_room, t_vent, currentTargetTemp, currentVentPosition);
    setVentOpening(newVentPosition);
    currentVentPosition = newVentPosition;
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

double getTargetTemp(){
  //Retrieve the current targetTemperature from the Firestore database for the unit
  //And return it
  if (Firebase.Firestore.getDocument(&fbdo2, FIREBASE_PROJECT_ID, "", "units/" + unitID, "targetTemperature")){
    //Serial.printf("ok\n%s\n\n", fbdo2.payload().c_str());
    JSONdata.clear();
    JSONdata.setJsonData(fbdo2.payload());
    FirebaseJsonData clear;
    result = clear;
    JSONdata.get(result, "fields/targetTemperature/integerValue");
    if (result.success && result.type != "null"){
      if (result.type == "string"){
        //Serial.println(result.to<String>().c_str());
        return std::stod(result.to<String>().c_str());
      }
      else if (result.type == "int"){
        //Serial.println(result.to<int>());
        return (int) result.to<int>();
      }
      else if (result.type == "float" || result.type == "double") {
        //Serial.println(result.to<double>());
        return result.to<double>();
      }
      else if (result.type == "null"){
        //Serial.println(result.to<String>().c_str());
        return -9999;
      }
    }
    else{
      JSONdata.get(result, "fields/targetTemperature/doubleValue");
      if (result.success){
        if (result.type == "float" || result.type == "double") {
          //Serial.println(result.to<double>());
          return result.to<double>();
        }
        else if (result.type == "string"){
          //Serial.println(result.to<String>().c_str());
          return std::stod(result.to<String>().c_str());
        }
        else if (result.type == "int"){
          //Serial.println(result.to<int>());
          return (int) result.to<int>();
        }

        else if (result.type == "null"){
          //Serial.println(result.to<String>().c_str());
          return -9999;
        }
      }
    }
  }
  else
      Serial.println(fbdo2.errorReason());
  return -9999;
}

void setVentOpening(int percent){
  //Set the servo to achieve the desired amount of vent open
  //100 percent is fully open vent
  //0 percent is fully closed vent
  if (percent>100)
     percent %= 100;
  int new_angle = (int) ((percent*1.4)+30);
  int current_angle = myservo.read();
  int pos=0;
  if (current_angle < new_angle){
    for (pos = current_angle; pos < new_angle; pos++) { // goes from 0 degrees to 180 degrees
      // in steps of 1 degree
      myservo.write(pos);              // tell servo to go to position in variable 'pos'
      delay(10);                       // waits 15 ms for the servo to reach the position
      }
    }
  else{
    for(pos = current_angle; pos > new_angle; pos--) { // goes from 180 degrees to 0 degrees
      myservo.write(pos);              // tell servo to go to position in variable 'pos'
      delay(10);                       // waits 15 ms for the servo to reach the position
    }
  }
}

int control_function(float room_temp, float vent_temp, float target_temp, int prev_state){
  float delta_temp_desired = abs(room_temp-target_temp); //difference in temp between target_temp and room_temp
  float threshold_temp = 0.2; //this is the threshold temperature that defines the range of temp around the target_temp that is allowed
  int percent_multiplier = 20; //this multiplier defines the amount the vent should be open per 0.1 degree in temp difference outside of threshold
  if (delta_temp_desired <= threshold_temp){
    return prev_state; //if within threshold of desired temp, keep vent at previous state
  }

  int percent_open = 0;

  if (vent_temp < room_temp && target_temp < room_temp){ //check if vent is cooler than room temp & check if cooling is needed
      percent_open = (int) floor((delta_temp_desired)*10) * percent_multiplier; //percent of vent that needs to be open according to percent_multiplier defined
      if (percent_open > 100){
        return 100; //percent cannot be greater than 100
      }
      return percent_open;
  }

  else if (vent_temp > room_temp && target_temp > room_temp) { //check if vent is hotter than room temp & check if heating is needed
      percent_open = (int) floor((delta_temp_desired)*10) * percent_multiplier; //percent of vent that needs to be open according to percent_multiplier defined
      if (percent_open > 100){
        return 100; //percent cannot be greater than 100
      }
      return percent_open;
  }

  else return 0; //in all other cases close the vent
}
