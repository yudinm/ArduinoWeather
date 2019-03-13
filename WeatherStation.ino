/*********
  Project: BME Weather Web server using NodeMCU
  Implements Adafruit's sensor libraries.
  Complete project is at: http://embedded-lab.com/blog/making-a-simple-weather-web-server-using-esp8266-and-bme280/
  
  Modified code from Rui Santos' Temperature Weather Server posted on http://randomnerdtutorials.com  
*********/
#define _DEBUG_
#define _DISABLE_TLS_

#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "Adafruit_Sensor.h"
#include "Adafruit_BME280.h"
#include <time.h>
#include <AutoConnect.h>
#include <ThingerWifi.h>

/*
 * MH-Z19B
 */
#include <SoftwareSerial.h>
#define pwmPin D8
SoftwareSerial swSerial(D6, D7); // RX, TX
unsigned long th, tl, ppm = 0, ppm2 = 0, ppm3 = 0, p1=0,p2=0, tpwm=0;


Adafruit_BME280 bme; // I2C

float h, t, p, pin, dp;
char temperatureFString[6];
char dpString[6];
char humidityString[6];
char pressureString[7];
char pressureInchString[6];

// Web Server on port 80
WiFiServer server(80);
ESP8266WebServer Server;
AutoConnect      Portal(Server);
AutoConnectConfig   Config;       // Enable autoReconnect supported on v0.9.4

#define TIMEZONE    (3600 * 9)    // Tokyo
#define NTPServer1  "ntp.nict.jp" // NICT japan.
#define NTPServer2  "time1.google.com"

ThingerWifi thing("mryu", "test1", "123456");

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
//  unsigned long th, tl, ppm = 0, ppm2 = 0, ppm3 = 0;

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

bool thingerConfigured = false;
// runs over and over again
void loop() {
  Portal.handleClient();
  if (WiFi.status() == WL_IDLE_STATUS) {
    ESP.reset();
    delay(1000);
  }

  if (WiFi.status() == WL_CONNECTED && !thingerConfigured) {
    // Printing the ESP IP address
    Serial.println(WiFi.localIP());
  
    if (!bme.begin()) {
      Serial.println("Could not find a valid BME280 sensor, check wiring!");
      while (1);
    }

    const char* ssid = WiFi.SSID().c_str();
    const char* psk = WiFi.psk().c_str();
    thing.add_wifi(ssid, psk);
    pinMode(LED_BUILTIN, OUTPUT);
    thing["led"] << digitalPin(LED_BUILTIN);
    thing["temperature"] >> outputValue(t);
    thing["humidity"] >> outputValue(h);
    thing["pressure"] >> outputValue(p);
    thing["co2ppm"] >> outputValue(ppm3);
    thingerConfigured = true;
    delay(10000); 
  }
  
  getWeather();
  getCo2ppm();
  thing.handle();
  
//  // Listenning for new clients
//  WiFiClient client = server.available();
//  
//  if (client) {
//    Serial.println("New client");
//    // bolean to locate when the http request ends
//    boolean blank_line = true;
//    while (client.connected()) {
//      if (client.available()) {
//        char c = client.read();
//        
//        if (c == '\n' && blank_line) {
//            
//            client.println("HTTP/1.1 200 OK");
//            client.println("Content-Type: text/html");
//            client.println("Connection: close");
//            client.println();
//            // your actual web page that displays temperature
//            client.println("<!DOCTYPE HTML>");
//            client.println("<html>");
//            client.println("<head><META HTTP-EQUIV=\"refresh\" CONTENT=\"15\"></head>");
//            client.println("<body><h1>ESP8266 Weather Web Server</h1>");
//            client.println("<table border=\"2\" width=\"456\" cellpadding=\"10\"><tbody><tr><td>");
//            client.println("<h3>Temperature = ");
//            client.println(temperatureFString);
//            client.println("&deg;C</h3><h3>Humidity = ");
//            client.println(humidityString);
//            client.println("%</h3><h3>Approx. Dew Point = ");
//            client.println(dpString);
//            client.println("&deg;C</h3><h3>Pressure = ");
//            client.println(pressureString);
//            client.println("hPa (");
//            client.println(pressureInchString);
//            client.println("Inch)</h3></td></tr></tbody></table></body></html>");  
//            break;
//        }
//        if (c == '\n') {
//          // when starts reading a new line
//          blank_line = true;
//        }
//        else if (c != '\r') {
//          // when finds a character on the current line
//          blank_line = false;
//        }
//      }
//    }  
//    // closing the client connection
//    delay(1);
//    client.stop();
//    Serial.println("Client disconnected.");
//  }
} 
