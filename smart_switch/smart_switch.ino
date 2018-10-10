#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "Timer.h"
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ThingSpeak.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "6e7a6ee669f44413b761b14cc9034616";


#define TRIGGER_PIN D3
const int RELAY1 = D7;
const int buzzer=D5;                        // Buzzer control port, default D5
WidgetLED led1(1); // On led

WiFiServer server(80);

char   host[] = "api.thingspeak.com"; // ThingSpeak address
String APIkey = "596819";             // Thingspeak Read Key, works only if a PUBLIC viewable channel
unsigned long channelID = 596819;
char *writeAPIKey = "GPBO64VFP5PRX1BP";
const int httpPort = 80;

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

void setup()
{

  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RELAY1, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT);

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

  Blynk.config(auth, "blynk.ogonan.com", 80);
  Blynk.connect(3333);
  ThingSpeak.begin( client );
  timer1.every(15000, RetrieveTSChannelData);
  timer2.every(60000, controlTemperature);
  timer.setInterval(15000L, sendStatus);

}

void loop()
{

  timer1.update();
  timer2.update();
  Blynk.run();
  timer.run();

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
  Serial.println(" Readings last updated at: "+datetime);

  for (int result = 0; result < max_result; result++){
    JsonObject& channel = root["feeds"][result]; // Now we can read 'feeds' values and so-on
    String entry_id     = channel["entry_id"];
    String field1value  = channel["field1"];
    Serial.print(" Field1 entry number ["+entry_id+"] had a value of: ");
    Serial.println(field1value);
    temperature = field1value.toFloat();
  }
}
  //Thing speak response to GET request(headers removed) and /result=1:
  //{"channel":{"id":320098,"name":"Test Channel","latitude":"0.0","longitude":"0.0","field1":"Pressure","field2":"Temperature","field3":"Humidity",
  //"created_at":"2017-08-21T13:22:12Z","updated_at":"2017-08-26T22:18:16Z","last_entry_id":85},
  // Second level [0] begins with array pointer [0] and then { } so we need to move down a level
  //"feeds":[{"created_at":"2017-08-26T22:18:16Z","entry_id":85,"field1":"40"}]}


void controlTemperature()
{
  Serial.println("Condition checking ...");
  if (temperature >= max_temperature) {
    if (digitalRead(RELAY1) == LOW) {
      turnRelayOn();
      Serial.println("Turn On");
      // sendThingSpeak();
    }
  }
  else if (temperature <= min_temperature) {
    if (digitalRead(RELAY1) == HIGH) {
      turnRelayOff();
      Serial.println("Turn Off");
      // sendThingSpeak();
    }
  }
  else {
    Serial.print("Temperature OK! : ");
    Serial.println(temperature);
  }
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

  Serial.print("Send temperature status :");
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

void sendThingSpeak()
{

    ThingSpeak.setField( 4,  relayStatus);


    int writeSuccess = ThingSpeak.writeFields( channelID, writeAPIKey );
    Serial.println(writeSuccess);
    Serial.println();
}
