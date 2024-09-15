// WIFI
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <string>

// Wifi Password
const char* ssid = "PDI";
const char* password = "69696969";

//Firebase
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#define API_KEY "AIzaSyBxDSuka_DTyCaCmiaJC6wK_jzuWo3CFW8"
#define DATABASE_URL "https://sensor-r35-default-rtdb.firebaseio.com/" 
unsigned long previousMillis = 0; 
const long interval = 10000; 

//Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

// MQ135
#include <TroykaMQ.h>
#define SENSOR_PIN        A0
MQ135 mq135(SENSOR_PIN);
float gs;

// Ultrasonic
const int TRIGPIN = 12;          
const int ECHOPIN = 14;
long timer;
int jarak;

// DHT
#include "DHT.h"
#define DHTPIN D7
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
float h;
float t;

void setup() {
  Serial.begin(9600);
// Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("connecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("connected: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  
// Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
// MQ135
  mq135.calibrate();  
  Serial.print("Ro = ");
  Serial.println(mq135.getRo());
  
// Ultrasonic
  pinMode(ECHOPIN, INPUT);
  pinMode(TRIGPIN, OUTPUT);
  
// DHT
  dht.begin();
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    fb();
    Serial.println("______________________________");
  }
}

void gas() {
  gs = mq135.readCO2();
}

void ultra() {
  digitalWrite(TRIGPIN, LOW);                   
  delayMicroseconds(2);
  digitalWrite(TRIGPIN, HIGH);                  
  delayMicroseconds(10);
  digitalWrite(TRIGPIN, LOW);                   

  timer = pulseIn(ECHOPIN, HIGH);
  jarak = timer/58;
}

void suhu() {
  h = dht.readHumidity();
  t = dht.readTemperature();
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
}

void fb(){
  gas();
  ultra();
  suhu();
  if (Firebase.ready() && signupOK ) {
    if (Firebase.RTDB.setFloat(&fbdo, "Sensor/humidity", h) && 
        Firebase.RTDB.setFloat(&fbdo, "Sensor/temperature", t) &&
        Firebase.RTDB.setFloat(&fbdo, "Sensor/gasLevel", gs) &&
        Firebase.RTDB.setFloat(&fbdo, "Sensor/jarak", jarak)) {
    
        Serial.print("Humidity: ");
        Serial.print(h);
        Serial.print("% |  Temperature: ");
        Serial.print(t);
        Serial.print("C |  GasLevel: ");
        Serial.print(gs);
        Serial.print("ppm |  Distance: ");
        Serial.print(jarak);
        Serial.print("cm");
        Serial.println("");
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
}
