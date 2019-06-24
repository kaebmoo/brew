#include <stdio.h>
#include <time.h>
#include <inttypes.h>

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "Timer.h"
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ThingSpeak.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <MicroGear.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "6e7a6ee669f44413b761b14cc9034616";


#define TRIGGER_PIN D3
#define WATCHDOG_PIN D6
// const int RELAY1 = D7;
const int RELAY1 = D1;
const int buzzer=D5;                        // Buzzer control port, default D5
WidgetLED led1(1); // On led

WiFiServer server(80);

char   host[] = "api.thingspeak.com"; // ThingSpeak address
String APIkey = "793984";             // Thingspeak Read Key, works only if a PUBLIC viewable channel, // brew channel temperature, humidity, battery

unsigned long channelID = 793986;
char *writeAPIKey = "VGLY9POK9GGK4WID"; // brew monitoring channel
const int httpPort = 80;

#define APPID   "Brew"
#define KEY     "3aewfy0NnL6pFnZ"
#define SECRET  "JrF5MRNuP8nC5uKQWAraykXiQ"
#define ALIAS   "ogoSwitch"

const char *ssid     = "Red";
const char *password = "12345678";
WiFiClient client;
const unsigned long HTTP_TIMEOUT = 10000;  // max respone time from server
Timer timer1, timer2;
BlynkTimer timer;
int max_result = 1;
float temperature = 19.0;
float max_temperature = 20.0;
float min_temperature = 19.0;
int relayStatus = 0;
int blynkStatus = 0;
int delayTime = 0;
unsigned long currenttime;
unsigned long t_lastUpdated;
bool blynkConnectedResult = false;
int blynkreconnect = 0;

// MicroGear microgear(client);

WiFiUDP ntpUDP;
// By default 'pool.ntp.org' is used with 60 seconds update interval and
// no offset
NTPClient timeClient(ntpUDP);

void setup()
{

  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RELAY1, OUTPUT);
  pinMode(WATCHDOG_PIN, OUTPUT);
  pinMode(buzzer, OUTPUT);

  digitalWrite(WATCHDOG_PIN, 0);


  /* Add Event listeners */
  /* Call onMsghandler() when new message arraives */
  // microgear.on(MESSAGE,onMsghandler);

  /* Call onFoundgear() when new gear appear */
  // microgear.on(PRESENT,onFoundgear);

  /* Call onLostgear() when some gear goes offline */
  // microgear.on(ABSENT,onLostgear);

  /* Call onConnected() when NETPIE connection is established */
  // microgear.on(CONNECTED,onConnected);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  wifiManager.setConfigPortalTimeout(60);
  if (!wifiManager.autoConnect("ogoSwitch")) {
    Serial.println("failed to connect, we should reset as see if it connects");
    delay(3000);
    ESP.reset();
    delay(5000);
  }
  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");


  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  /*
  WiFi.begin(ssid,password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    // if ( digitalRead(TRIGGER_PIN) == LOW ) {
    //  ondemandWiFi();
    // }
  }

  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
  */

  timeClient.begin();
  Blynk.config(auth, "blynk.ogonan.com", 80);
  Blynk.connect(3333);
  blynkStatus = Blynk.connected();
  ThingSpeak.begin( client );
  /* Initial with KEY, SECRET and also set the ALIAS here */
  // microgear.init(KEY,SECRET,ALIAS);

  /* connect to NETPIE to a specific APPID */
  // microgear.connect(APPID);

  timer1.every(60000, RetrieveTSChannelData);
  timer2.every(60000, controlTemperature);
  timer.setInterval(15000L, sendStatus);
  timer.setInterval(60000L, checkBlynkConnection);
  timer.setInterval(60000L, resetWatchdog);
  
  // timer.setInterval(15000L, checkMicrogearConnection);
 
  // update time 
  timeClient.update();
  currenttime = timeClient.getEpochTime();
  
  
  RetrieveTSChannelData();
  controlTemperature();
  sendStatus();
}

void loop()
{
  // update time 
  timeClient.update();
  currenttime = timeClient.getEpochTime();

  // scheduling 
  timer1.update();
  timer2.update();
  Blynk.run();
  timer.run();

  /*
  if (microgear.connected()) {
    microgear.loop();
    delayTime = 0;
  }
  else {
      if (delayTime >= 5000) {
          Serial.println("Netpie connection lost, reconnect...");
          microgear.connect(APPID);
          delayTime = 0;
      }
      else {
        delayTime += 100;
      }
    delay(100);
  }
  */
}


void RetrieveTSChannelData() {  // Receive data from Thingspeak
  static char responseBuffer[3*1024]; // Buffer for received data
  client = server.available();
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  String url = "/channels/" + APIkey; // Start building API request string
  url += "/fields/1.json?results=1";  // 5 is the results request number, so 5 are returned, 1 woudl return the last result received
  client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
  while (!skipResponseHeaders());                      // Wait until there is some data and skip headers
  while (client.available()) {                         // Now receive the data
    String line = client.readStringUntil('\n');
    if ( line.indexOf('{',0) >= 0 ) {                  // Ignore data that is not likely to be JSON formatted, so must contain a '{'
      Serial.println(line);                            // Show the text received
      line.toCharArray(responseBuffer, line.length()); // Convert to char array for the JSON decoder
      decodeJSON(responseBuffer);                      // Decode the JSON text
    }
  }
  client.stop();
}

bool skipResponseHeaders() {
  char endOfHeaders[] = "\r\n\r\n"; // HTTP headers end with an empty line
  client.setTimeout(HTTP_TIMEOUT);
  bool ok = client.find(endOfHeaders);
  if (!ok) { Serial.println("No response or invalid response!"); }
  return ok;
}

bool decodeJSON(char *json) {
  StaticJsonBuffer <3*1024> jsonBuffer;
  char *jsonstart = strchr(json, '{'); // Skip characters until first '{' found and ignore length, if present
  if (jsonstart == NULL) {
    Serial.println("JSON data missing");
    return false;
  }
  json = jsonstart;
  JsonObject& root = jsonBuffer.parseObject(json); // Parse JSON
  if (!root.success()) {
    Serial.println(F("jsonBuffer.parseObject() failed"));
    return false;
  }
  JsonObject& root_data = root["channel"]; // Begins and ends within first set of { }
  String id   = root_data["id"];
  String name = root_data["name"];
  String field1_name = root_data["field1"];
  String datetime    = root_data["updated_at"];
  Serial.println("\n\n Channel id: "+id+" Name: "+ name);
  Serial.println(" Channel last updated at: "+datetime);

  for (int result = 0; result < max_result; result++){
    JsonObject& channel = root["feeds"][result]; // Now we can read 'feeds' values and so-on
    String lastUpdated  = channel["created_at"];
    String entry_id     = channel["entry_id"];
    String field1value  = channel["field1"];
    Serial.print(" Feeds last updated at: ");
    Serial.print(lastUpdated);
    Serial.print(" Field1 entry number ["+entry_id+"] had a value of: ");
    Serial.println(field1value);
    temperature = field1value.toFloat();

    char str_buf[] = "2018-10-10T13:39:50Z";
    memset(str_buf, 0, sizeof(str_buf));
    // Serial.println(strlen(lastUpdated.c_str()));
    // Serial.println(strlen(str_buf));
    if(strlen(lastUpdated.c_str()) <= 20) {
      strcpy(str_buf, lastUpdated.c_str());
      t_lastUpdated = human2Epoch(str_buf);
      Serial.print(" Epoch last updated: ");
      Serial.print(t_lastUpdated);
      Serial.print(" Current time: ");
      Serial.println(currenttime);
    }
  }
}
  //Thing speak response to GET request(headers removed) and /result=1:
  //{"channel":{"id":320098,"name":"Test Channel","latitude":"0.0","longitude":"0.0","field1":"Pressure","field2":"Temperature","field3":"Humidity",
  //"created_at":"2017-08-21T13:22:12Z","updated_at":"2017-08-26T22:18:16Z","last_entry_id":85},
  // Second level [0] begins with array pointer [0] and then { } so we need to move down a level
  //"feeds":[{"created_at":"2017-08-26T22:18:16Z","entry_id":85,"field1":"40"}]}


void controlTemperature()
{
  String message = "";
  
  Serial.println("Condition checking ...");
  Serial.print(" Epoch last updated time: ");
  Serial.println(t_lastUpdated);
  Serial.print("Current time: ");
  Serial.println(currenttime);

  if ((currenttime - t_lastUpdated) > 7200) {
    Serial.println("Temperature data is not updated for 2 hours.");
  }

  if (temperature >= max_temperature) {
    Serial.println("High Temperature");
    message = "High Temperature, ";
    if (digitalRead(RELAY1) == LOW) {
      turnRelayOn();
      Serial.println("Turn On");
      sendThingSpeak();      
    }
  }
  else if (temperature <= min_temperature) {
    Serial.println("Low Temperature");
    message = "Low Temperature, ";
    if (digitalRead(RELAY1) == HIGH) {
      turnRelayOff();
      Serial.println("Turn Off");
      sendThingSpeak();
    }
  }
  else {
    Serial.print("Temperature is in range.");
    Serial.println(temperature);
    message = "Temperature is in range, ";
    
  }

  // send data to netpie
  /*
  String status = message + (String) temperature + ", min: " + (String) min_temperature + ", max: " + (String) max_temperature;
  Serial.println("Publish status to netpie: " + status);
  if (!microgear.connected()) {
    microgear.connect(APPID);
    microgear.publish("/brew/temperature/status", status, true);
  }
  else {
    microgear.publish("/brew/temperature/status", status, true);
  }
  */
    
}

void ondemandWiFi()
{
    WiFiManager wifiManager;

    if (!wifiManager.startConfigPortal("ogoSense")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");

}

void turnRelayOn()
{
  digitalWrite(RELAY1, HIGH);
  Serial.println("RELAY1 ON");
  digitalWrite(LED_BUILTIN, HIGH);  // turn on
  led1.on();
  // Blynk.virtualWrite(V1, 1);
  buzzer_sound();
  relayStatus = 1;
}

void turnRelayOff()
{
  digitalWrite(RELAY1, LOW);
  Serial.println("RELAY1 OFF");
  digitalWrite(LED_BUILTIN, LOW);  // turn off
  led1.off();  // blynk led
  // Blynk.virtualWrite(V1, 0);
  buzzer_sound();
  relayStatus = 0;
}


void buzzer_sound()
{
  analogWriteRange(1047);
  analogWrite(buzzer, 512);
  delay(100);
  analogWrite(buzzer, 0);
  pinMode(buzzer, OUTPUT);
  digitalWrite(buzzer, LOW);
  delay(100);

  analogWriteRange(1175);
  analogWrite(buzzer, 512);
  delay(300);
  analogWrite(buzzer, 0);
  pinMode(buzzer, OUTPUT);
  digitalWrite(buzzer, LOW);
  delay(300);
}

void sendStatus()
{
  if (relayStatus) {
    led1.on();
  }
  else {
    led1.off();
  }
  Serial.print("Send temperature status to blynk: ");
  Serial.println(temperature);
  Blynk.virtualWrite(V2, temperature);
}

BLYNK_WRITE(V3)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V3 to a variable
  // You can also use:
  // String i = param.asStr();
  // double d = param.asDouble();
  Serial.print("V3 Step H value is: ");
  Serial.println(pinValue);
  min_temperature = param.asFloat();
}

BLYNK_WRITE(V4)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V4 to a variable
  // You can also use:
  // String i = param.asStr();
  // double d = param.asDouble();
  Serial.print("V4 Step H value is: ");
  Serial.println(pinValue);
  max_temperature = param.asFloat();
}

BLYNK_CONNECTED()
{
  Serial.println("Blynk Connected");
  // Blynk.syncAll();

  int relay_status = digitalRead(RELAY1);

  if (relay_status == 1) {
    led1.on();
  }
  else {
    led1.off();
  }

  Blynk.syncVirtual(V3);  // min temperature
  Blynk.syncVirtual(V4);  // max temperature

}

void checkBlynkConnection()
{
  int mytimeout;

  Serial.println("Check Blynk connection.");
  blynkConnectedResult = Blynk.connected();
  if (!blynkConnectedResult) {
    Serial.println("Blynk not connected");
    mytimeout = millis() / 1000;
    Serial.println("Blynk trying to reconnect.");
    while (!blynkConnectedResult) {
      blynkConnectedResult = Blynk.connect(3333);
      Serial.print(".");
      if((millis() / 1000) > mytimeout + 3) { // try for less than 4 seconds
        Serial.println("Blynk reconnect timeout.");
        break;
      }
    }
  }
  if (blynkConnectedResult) {
      Serial.println("Blynk connected");
  }
  else {
    Serial.println("Blynk not connected");
    Serial.print("blynkreconnect: ");
    Serial.println(blynkreconnect);
    blynkreconnect++;
    if (blynkreconnect >= 10) {
      blynkreconnect = 0;
      // delay(60000);
      // ESP.reset();
    }
  }
}

/*
void checkMicrogearConnection()
{
  if (microgear.connected()) {
    Serial.println("connected");
  }
  else {
    Serial.println("connection lost, reconnect...");
    microgear.connect(APPID);
  }
}
*/

void sendThingSpeak()
{

    ThingSpeak.setField( 4,  relayStatus);


    int writeSuccess = ThingSpeak.writeFields( channelID, writeAPIKey );
    Serial.print("Return code from ThingSpeak: ");
    Serial.println(writeSuccess);
    Serial.println();
}


/* If a new message arrives, do this */
void onMsghandler(char *topic, uint8_t* msg, unsigned int msglen) {
    Serial.print("Incoming message --> ");
    msg[msglen] = '\0';
    Serial.println((char *)msg);

    Serial.print("Topic: ");
    Serial.println(topic);

    if (strcmp(topic, "/Brew/brew/switch") == 0) {
      if ((char)msg[0] == '1') {
        Serial.println("Turn Relay ON.");
        turnRelayOn();
      }
      else if ((char)msg[0] == '0') {
        Serial.println("Turn Relay OFF.");
        turnRelayOff();
      }
    }
}

void onFoundgear(char *attribute, uint8_t* msg, unsigned int msglen) {
    Serial.print("Found new member --> ");
    for (int i=0; i<msglen; i++)
        Serial.print((char)msg[i]);
    Serial.println();
}

void onLostgear(char *attribute, uint8_t* msg, unsigned int msglen) {
    Serial.print("Lost member --> ");
    for (int i=0; i<msglen; i++)
        Serial.print((char)msg[i]);
    Serial.println();
}

/* When a microgear is connected, do this */
/*
void onConnected(char *attribute, uint8_t* msg, unsigned int msglen) {
    Serial.println("Connected to NETPIE...");
    // Set the alias of this microgear ALIAS 
    microgear.setAlias(ALIAS);
    microgear.subscribe("/brew/temperature");
    microgear.subscribe("/brew/switch");
}
*/

long human2Epoch(char str_buf[21])
{

  struct tm t;
  time_t t_of_day;
  //char str_buf[] = "2018-10-10T13:39:50Z";
  uintmax_t year, month, day, hour, min, sec;
  char c_year[5], c_month[3], c_day[3], c_hour[3], c_min[3], c_sec[3];

  c_year[0] = str_buf[0];
  c_year[1] = str_buf[1];
  c_year[2] = str_buf[2];
  c_year[3] = str_buf[3];
  c_year[4] = '\0';
  c_month[0] = str_buf[5];
  c_month[1] = str_buf[6];
  c_month[2] = '\0';
  c_day[0] = str_buf[8];
  c_day[1] = str_buf[9];
  c_day[2] = '\0';
  c_hour[0] = str_buf[11];
  c_hour[1] = str_buf[12];
  c_hour[2] = '\0';
  c_min[0] = str_buf[14];
  c_min[1] = str_buf[15];
  c_min[2] = '\0';
  c_sec[0] = str_buf[17];
  c_sec[1] = str_buf[18];
  c_sec[2] = '\0';

  /*
  year = strtoumax(c_year, NULL, 10);
  month = strtoumax(c_month, NULL, 10);
  day = strtoumax(c_day, NULL, 10);
  hour = strtoumax(c_hour, NULL, 10);
  min = strtoumax(c_min, NULL, 10);
  sec = strtoumax(c_sec, NULL, 10);
  *
  *
   */

   year = atoi(c_year);
   month = atoi(c_month);
   day = atoi(c_day);
   hour = atoi(c_hour);
   min = atoi(c_min);
   sec = atoi(c_sec);

  t.tm_year = year-1900;
  t.tm_mon = month-1;           // Month, 0 - jan
  t.tm_mday = day;          // Day of the month
  t.tm_hour = hour;
  t.tm_min = min;
  t.tm_sec = sec;
  t.tm_isdst = -1;        // Is DST on? 1 = yes, 0 = no, -1 = unknown
  t_of_day = mktime(&t);

  // printf("seconds since the Epoch: %ld\n", (long) t_of_day);

  return((long) t_of_day);

}


void resetWatchdog() 
{
  Serial.println("Watchdog reset");
  digitalWrite(WATCHDOG_PIN, HIGH);
  delay(20);
  digitalWrite(WATCHDOG_PIN, LOW);
}
