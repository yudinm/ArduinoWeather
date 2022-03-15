/*********
  Project: BME Weather Web server using NodeMCU
  Implements Adafruit's sensor libraries.
  Complete project is at: http://embedded-lab.com/blog/making-a-simple-weather-web-server-using-esp8266-and-bme280/
  
  Modified code from Rui Santos' Temperature Weather Server posted on http://randomnerdtutorials.com  
*********/
#define _DEBUG_
#define _DISABLE_TLS_

//#define _HAS_Z19B_CO2_SENSOR_
#define _HAS_BME280_WEATHER_SENSOR_

#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "Adafruit_Sensor.h"
#include "Adafruit_BME280.h"
#include <time.h>
#include <AutoConnect.h>
#include <ESP8266HTTPClient.h> 

//#include <WiFiClient.h>
//#include <ThingerWifi.h>

/*
 * MH-Z19B
 */
#include <SoftwareSerial.h>
#define pwmPin D8
SoftwareSerial swSerial(D6, D7); // RX, TX
unsigned long th, tl, ppm = 0, ppm2 = 0, ppm3 = 0, p1=0, p2=0, tpwm=0;

Adafruit_BME280 bme; // I2C

float h, t, p, pin, dp;
char temperatureFString[6];
char dpString[6];
char humidityString[6];
char pressureString[7];
char pressureInchString[6];

// Web Server on port 80
WiFiServer          server(80);
ESP8266WebServer    Server;
AutoConnect         Portal(Server);
AutoConnectConfig   Config;       // Enable autoReconnect supported on v0.9.4

#define TIMEZONE    (3600 * 3)    // Moscow
#define NTPServer1  "0.ru.pool.ntp.org" // 
#define NTPServer2  "time1.google.com"

const char host[]     = "192.168.31.160";//"10.16.84.182"; //"10.16.84.182"; //"vapor-weather.herokuapp.com";//"10.16.84.182"; //
const int httpsPort   = 80;
WiFiClient client;//(host, 443);
#define SamplingDelay (1000*6*3)
const char* fingerprint = "08 3B 71 72 02 43 6E CA ED 42 86 93 BA 7E DF 81 C4 BC 62 30";

const String device_id    = "T_P_H_1";
const String device_name  = "Weather_station_TPH1";


void rootPage() {
  String  content = 
    "<html>"
    "<head>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "</head>"
    "<body>"
    "<h2 align=\"center\" style=\"color:blue;margin:20px;\">Hello, world</h2>"
    "<h3 align=\"center\" style=\"color:gray;margin:10px;\">{{DateTime}}</h3>"
    "<p style=\"padding-top:10px;text-align:center\">" AUTOCONNECT_LINK(COG_32) "</p>"
    "</body>"
    "</html>";
  static const char *wd[7] = { "Sun","Mon","Tue","Wed","Thr","Fri","Sat" };
  struct tm *tm;
  time_t  t;
  char    dateTime[26];

  t = time(NULL);
  tm = localtime(&t);
  sprintf(dateTime, "%04d/%02d/%02d(%s) %02d:%02d:%02d.",
    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
    wd[tm->tm_wday],
    tm->tm_hour, tm->tm_min, tm->tm_sec);
  content.replace("{{DateTime}}", String(dateTime));
  Server.send(200, "text/html", content);
}

// only runs once on boot
void setup() {

  // Initializing serial port for debugging purposes
  Serial.begin(9600);
  delay(1000);
  Wire.begin(D1, D2);
  Wire.setClock(100000);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_BUILTIN, LOW);
  
#ifdef _HAS_Z19B_CO2_SENSOR_  
  swSerial.begin(9600); 
  th = pulseIn(pwmPin, HIGH, 3000000); // use microseconds
  tl = pulseIn(pwmPin, LOW, 3000000);
  tpwm = th + tl; // actual pulse width
//  Serial.print("PWM-time: ");
//  Serial.print(tpwm);
//  Serial.println(" us");
  p1 = tpwm/502; // start pulse width
  p2 = tpwm/251; // start and end pulse width combined
  th = pulseIn(pwmPin, HIGH, 3000000);
  ppm3 = 5000 * (th-p1)/(tpwm-p2);
  
  byte setrangeA_cmd[9] = {0xFF, 0x01, 0x99, 0x00, 0x00, 0x00, 0x13, 0x88, 0xCB}; // задаёт диапазон 0 - 5000ppm
  unsigned char setrangeA_response[9]; 
  swSerial.write(setrangeA_cmd,9);
  swSerial.readBytes(setrangeA_response, 9);
  int setrangeA_i;
  byte setrangeA_crc = 0;
  for (setrangeA_i = 1; setrangeA_i < 8; setrangeA_i++) setrangeA_crc+=setrangeA_response[setrangeA_i];
  setrangeA_crc = 255 - setrangeA_crc;
  setrangeA_crc += 1;
  if ( !(setrangeA_response[0] == 0xFF && setrangeA_response[1] == 0x99 && setrangeA_response[8] == setrangeA_crc) ) {
    Serial.println("Range CRC error: " + String(setrangeA_crc) + " / "+ String(setrangeA_response[8]) + " (bytes 6 and 7)");
  } else {
    Serial.println("Range was set! (bytes 6 and 7)");
  }
#endif
  // Behavior a root path of ESP8266WebServer.
//  Server.on("/", rootPage);

  // Enable saved past credential by autoReconnect option,
  // even once it is disconnected.
  Config.autoReconnect = true;
  Portal.config(Config);

  // Establish a connection with an autoReconnect option.
  if (Portal.begin()) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
    configTime(TIMEZONE, 0, NTPServer1, NTPServer2);

    // Starting the web server
    server.begin();
    Serial.println("Web server running. Waiting for the ESP IP...");
    delay(10000);
    
  } else {
    Serial.println("WiFi connection failed.");
  }
}

void getWeather() {
#ifndef _HAS_BME280_WEATHER_SENSOR_
  Serial.println("BME280 weather sensor disabled…");
  return;
#endif
    h = bme.readHumidity();
    t = bme.readTemperature();
//    t = t*1.8+32.0;
    dp = t-(1.0-h*0.01)/0.05;
    
    p = bme.readPressure()/100.0F;
    pin = 0.02953*p;
    dtostrf(t, 5, 1, temperatureFString);
    dtostrf(h, 5, 1, humidityString);
    dtostrf(p, 6, 1, pressureString);
    dtostrf(pin, 5, 2, pressureInchString);
    dtostrf(dp, 5, 1, dpString);
    delay(100);
 
}

void getCo2ppm() {
#ifndef _HAS_Z19B_CO2_SENSOR_
  Serial.println("Z19B_CO2 sensor disabled…");
  return;
#endif
  th = pulseIn(pwmPin, HIGH, 3000000); // use microseconds
  tl = pulseIn(pwmPin, LOW, 3000000);
  tpwm = th + tl; // actual pulse width
//  Serial.print("PWM-time: ");
//  Serial.print(tpwm);
//  Serial.println(" us");
  p1 = tpwm/502; // start pulse width
  p2 = tpwm/251; // start and end pulse width combined
  th = pulseIn(pwmPin, HIGH, 3000000);
  ppm = 5000 * (th-p1)/(tpwm-p2);

  byte measure_cmd[9] = {0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79};
  unsigned char measure_response[9]; 

  // ***** узнаём концентрацию CO2 через UART: ***** 
  swSerial.write(measure_cmd,9);
  swSerial.readBytes(measure_response, 9);
  int i;
  byte crc = 0;
  for (i = 1; i < 8; i++) crc+=measure_response[i];
  crc = 255 - crc;
  crc += 1;
  if ( !(measure_response[0] == 0xFF && measure_response[1] == 0x86 && measure_response[8] == crc) ) {
    Serial.println("CRC error: " + String(crc) + " / "+ String(measure_response[8]));
  } 
  unsigned int responseHigh = (unsigned int) measure_response[2];
  unsigned int responseLow = (unsigned int) measure_response[3];
  ppm = (256*responseHigh) + responseLow;

  // *****  узнаём концентрацию CO2 через PWM: ***** 
  int waitCounter = 1;
  do {
    th = pulseIn(pwmPin, HIGH, 1004000) / 1000;
//    th = digitalRead(pwmPin);
    tl = 1004 - th;
//    ppm2 =  2000 * (th-2)/(th+tl-4); // расчёт для диапазона от 0 до 2000ppm 
    ppm3 =  5000 * (th-2)/(th+tl-4); // расчёт для диапазона от 0 до 5000ppm 
    waitCounter++;
    if (waitCounter > 5) {
      waitCounter = -1;
    }
    Serial.print("w..");
    Serial.println(waitCounter);

  } while (th == 0 && waitCounter > 0);

  Serial.print("PPM: ");
  Serial.println(ppm);
  Serial.print("PPM3: ");
  Serial.println(ppm3);

}

void post() {
  Serial.print("connecting to host: ");
  Serial.println(host);
//  Serial.printf("Using fingerprint '%s'\n", fingerprint);
//  client.setFingerprint(fingerprint);
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }
  Serial.print("Connected to: ");
  Serial.println(host);

  HTTPClient http;
  http.begin("http://" + String(host) + ":80/sensors/push_data"); 
  http.addHeader("Content-Type", "application/x-www-form-urlencoded"); 
  
  String postData="device_id=" + device_id + "&name=" + device_name + "&data=";
  postData = postData + "{";
#ifdef _HAS_BME280_WEATHER_SENSOR_
  postData = postData + "\"hum\":" + String(h) + ",";
  postData = postData + "\"tem\":" + String(t) + ",";
  postData = postData + "\"pre\":" + String(p);
#ifdef _HAS_Z19B_CO2_SENSOR_
  postData = postData + ",";
#endif  
#endif
#ifdef _HAS_Z19B_CO2_SENSOR_
  postData = postData + "\"co2\":" + String(ppm3);
#endif
  postData = postData + "}";
  Serial.println(postData); 
   
  auto httpCode = http.POST(postData); 
  Serial.println(httpCode); //Print HTTP return code 

  String payload = http.getString(); 
  Serial.println(payload); //Print request response payload 

//  while (client.connected()) {
//    String line = client.readStringUntil('\n');
//    if (line == "\r") {
//      Serial.println("headers received");
//      break;
//    }
//  }
//  String line = client.readStringUntil('\n'); 
//  Serial.println(line);

  // Close the connection
  Serial.println();
  http.end(); //Close connection Serial.println(); 
  Serial.println("closing connection");

  client.stop();
}

//bool thingerConfigured = false;
// runs over and over again
void loop() {
  Portal.handleClient();
  
  if (WiFi.status() == WL_IDLE_STATUS) {
    ESP.reset();
    delay(1000);
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Printing the ESP IP address
    Serial.println(WiFi.localIP());
  
    if (!bme.begin()) {
      Serial.println("Could not find a valid BME280 sensor, check wiring!");
      pinMode(LED_BUILTIN, HIGH);
      while (1);
    }
    delay(SamplingDelay); 
  }

  getWeather();
  getCo2ppm();
  post(); 
  pinMode(LED_BUILTIN, LOW);
} 
