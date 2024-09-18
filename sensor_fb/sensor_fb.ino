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
int currentNumber = 1;
unsigned long previousMillis = 0; 
const long interval = 10000; 

// RT
#include <NTPClient.h>
#include <WiFiUdp.h>
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200, 60000); // Waktu dalam zona WIB (UTC +7)


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
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // NTP Client setup
  timeClient.begin();
  timeClient.update();

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

void fb() {
  gas();
  ultra();
  suhu();

  // Perbarui waktu dari NTP
  timeClient.update();

  // Dapatkan detail waktu
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  int currentSecond = timeClient.getSeconds();

  // Mendapatkan tanggal, bulan, tahun dari epoch time
  time_t rawTime = timeClient.getEpochTime();
  struct tm* timeInfo = localtime(&rawTime);
  int currentDay = timeInfo->tm_mday;
  int currentMonth = timeInfo->tm_mon + 1; // Karena bulan dimulai dari 0, jadi tambahkan 1
  int currentYear = timeInfo->tm_year + 1900; // Tahun dimulai dari 1900, jadi tambahkan 1900

  // Format waktu dan tanggal menjadi string yang lebih informatif
  String timestamp = String(currentHour) + ":" + String(currentMinute) + ":" + String(currentSecond) + "   " +
                     String(currentDay) + "-" + String(currentMonth) + "-" + String(currentYear);

  // Cek apakah currentNumber sudah ada di Firebase
  if (Firebase.RTDB.getInt(&fbdo, "/currentNumber")) {
    currentNumber = fbdo.intData();
    Serial.println("Current number retrieved: " + String(currentNumber));
  } else {
    // Jika path belum ada, inisialisasi currentNumber dengan 1
    Serial.println("Path not exist, initializing current number to 1.");
    currentNumber = 1;
    if (!Firebase.RTDB.setInt(&fbdo, "/currentNumber", currentNumber)) {
      Serial.println("FAILED to initialize current number");
      Serial.println("REASON: " + fbdo.errorReason());
      return; // Jika gagal, hentikan proses
    }
  }

  // Buat path berdasarkan nomor urut
  String path = "/Sensor/" + String(currentNumber);

  if (Firebase.ready() && signupOK) {
    if (Firebase.RTDB.setFloat(&fbdo, path + "/humidity", h) && 
        Firebase.RTDB.setFloat(&fbdo, path + "/temperature", t) &&
        Firebase.RTDB.setFloat(&fbdo, path + "/gasLevel", gs) &&
        Firebase.RTDB.setFloat(&fbdo, path + "/jarak", jarak) &&
        Firebase.RTDB.setString(&fbdo, path + "/timestamp", timestamp)) {

        Serial.print("Data sent to path: ");
        Serial.println(path);
        Serial.print("Tanggal: ");
        Serial.print(timestamp);
        Serial.print(" |  Humidity: ");
        Serial.print(h);
        Serial.print("% |  Temperature: ");
        Serial.print(t);
        Serial.print("C |  GasLevel: ");
        Serial.print(gs);
        Serial.print("ppm | Jarak: ");
        Serial.print(jarak);
        Serial.println("cm");

        // Tingkatkan nomor urut dan simpan kembali di Firebase
        currentNumber++;
        if (!Firebase.RTDB.setInt(&fbdo, "/currentNumber", currentNumber)) {
          Serial.println("FAILED to update current number");
          Serial.println("REASON: " + fbdo.errorReason());
        }
    } 
    else {
      Serial.println("FAILED to send data");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
}
