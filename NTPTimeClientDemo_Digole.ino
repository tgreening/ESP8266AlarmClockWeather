#include <sampledata.h>

#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManagerOLED.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <NTPTimeClient.h>
#include <ESP8266WiFi.h>
#include "WundergroundClient.h"
#include "Time.h"
#include "images.h"
#include <SPI.h>
#include <Ethernet.h>
#include <ESP8266mDNS.h>
#include "FS.h"
#include <LinkedList.h>
#include <ArduinoJson.h>
#include <DFPlayer_Mini_Mp3.h>
#include <SoftwareSerial.h>
#define _Digole_Serial_I2C_  //To tell compiler compile the special communication only, 

#include <DigoleSerial.h>
DigoleSerialDisp mydisp(&Wire, '\x27'); //I2C:Arduino UNO: SDA (data line) is on analog input pin 4, and SCL (clock line) is on analog input pin 5 on UNO and Duemilanove


#define PIN_BUSY 13
#define PIR_PIN 2
#define SDC_PIN 14
#define SDA_PIN 12

boolean busy = true;
int pirState = LOW;             // we start, assuming no motion detected
boolean offset = false;

// Wunderground Settings
const boolean IS_METRIC = false;
const String WUNDERGRROUND_API_KEY = "f01878796b8afea5";
const String WUNDERGRROUND_LANGUAGE = "EN";
const String WUNDERGROUND_COUNTRY = "MI";
const String WUNDERGROUND_CITY = "Birmingham";

// Set to false, if you prefere imperial/inches, Fahrenheit
WundergroundClient wunderground(IS_METRIC);

SoftwareSerial mp3Serial(12, 14); // RX, TX

struct alarm {
  int alarmDay;
  int alarmHour;
  int alarmMinute;
  int alarmEnabled;
};
LinkedList<alarm*> alarms = LinkedList<alarm*>();
LinkedList<alarm*> alarmsToday = LinkedList<alarm*>();
String apiKey = "H4ND8D2YBT0GR8KK";
const char* thingSpeakserver = "api.thingspeak.com";
WiFiClient client;
NTPTimeClient timeClient;
WiFiManagerOLED wifiManager;


NTPClient* ntpClient;
ESP8266WebServer server(80);
const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
const char* host = "esp8266-webupdate";
boolean motion = false;
int savedDayOfWeek = -1;
unsigned long startMinute;
bool checkedHttp = false;

const char* html = "<html>\n"
                   "<head>\n"
                   "<title>Alarms</title>\n"
                   "<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.1.1/jquery.min.js\"></script>\n"
                   "</head>\n"
                   "<body onload=\"getAlarms()\">\n"
                   "<form id=\"frmAlarms\" method=\"POST\" action=\"/update\">\n"
                   "<div id=\"formElems\" name=\"formElems\"></div>\n"
                   "<input type=\"hidden\" name=\"noAlarms\" id=\"noAlarms\" value=0 />\n"
                   "<a href=\"#\" onclick=\"addNewAlarm()\">Add</a>&nbsp;&nbsp;"
                   "<a href=\"#\" onclick=\"document.forms['frmAlarms'].submit();\">Submit</a>\n"
                   "<br>\n"
                   "</form>\n"
                   "<script>\n"
                   "$('form .submit-link').on({"
                   "    click: function (event) {\n"
                   "        event.preventDefault();\n"
                   "        $(this).closest('form').submit();\n"
                   "    }\n"
                   "});\n"

                   "function addNewAlarm() {\n"
                   " var noAlarmsElem = document.getElementById('noAlarms');\n"
                   " var noAlarms = parseInt(noAlarmsElem.value);\n"
                   " var div = document.createElement(\"DIV\");\n"
                   " div.innerHTML = addAlarm(noAlarms);\n"
                   " noAlarmsElem.value = (++noAlarms).toString();\n"
                   " var formElem = document.getElementById('formElems');\n"
                   " formElem.append(div);\n"
                   "}\n"

                   "function getAlarms() {\n"
                   " $(document).ready(function() {\n"
                   "  $.ajax({\n"
                   "  url: \"/data\"\n"
                   "  }).then(function(data) {\n"
                   "   var i = 0;\n"
                   "   var htmlString = \"\";\n"
                   "   $.each(data, function(key, val) {\n"
                   "     htmlString += addAlarm(i);\n"
                   "     i++;\n"
                   "   });\n"
                   "   var formElem = document.getElementById('formElems');\n"
                   "   formElem.innerHTML = htmlString;\n"
                   "   document.getElementById('noAlarms').value = i.toString();\n"
                   "   i = 0;\n"
                   "   $.each(data, function(key, val) {\n"
                   "     document.getElementById('day' + i).value = this.day;\n"
                   "     document.getElementById('hour' + i).value = this.hour;\n"
                   "     document.getElementById('minute' + i).value = this.minute;\n"
                   "     document.getElementById('active' + i++).value = this.enabled;\n"
                   "   });\n"
                   "  });\n"
                   " });"
                   "}\n"

                   "function deleteAlarm(alarmNo) {\n"
                   " document.getElementById('alarm' + alarmNo).style.display = 'none';\n"
                   " document.getElementById('delete' + alarmNo).value = 1;\n"
                   "}\n"

                   "function addAlarm(alarmNo) {"
                   "  var htmlString = \"\";\n"
                   " htmlString +=\"<div id =\\\"alarm\" + alarmNo + \"\\\">\";\n"
                   " htmlString +=\"<select id=\\\"day\" + alarmNo + \"\\\" name=\\\"day\" + alarmNo + \"\\\">\\\n\";\n"
                   " htmlString +=\"<option value=\\\"1\\\">Sunday</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"2\\\">Monday</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"3\\\">Tuesday</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"4\\\">Wednesday</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"5\\\">Thursday</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"6\\\">Friday</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"7\\\">Saturday</option>\\n\";\n"
                   " htmlString +=\"</select>\";\n"
                   " htmlString +=\"<select id=\\\"hour\" + alarmNo + \"\\\" name=\\\"hour\" + alarmNo + \"\\\">\\n\";\n"
                   " htmlString +=\"<option value=\\\"0\\\">12 AM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"1\\\">1 AM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"2\\\">2 AM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"3\\\">3 AM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"4\\\">4 AM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"5\\\">5 AM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"6\\\">6 AM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"7\\\">7 AM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"8\\\">8 AM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"9\\\">9 AM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"10\\\">10 AM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"11\\\">11 AM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"12\\\">12 PM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"13\\\">1 PM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"14\\\">2 PM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"15\\\">3 PM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"16\\\">4 PM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"17\\\">5 PM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"18\\\">6 PM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"19\\\">7 PM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"20\\\">8 PM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"21\\\">9 PM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"22\\\">10 PM</option>\\n\";\n"
                   " htmlString +=\"<option value=\\\"23\\\">11 PM</option>\\n\";\n"
                   " htmlString +=\"</select>\\n\";\n"
                   " htmlString +=\"<input type=\\\"number\\\" id=\\\"minute\" + alarmNo + \"\\\" name=\\\"minute\" + alarmNo + \"\\\" placeholder=\\\"minute\\\"  size=\\\"2\\\" maxlength=\\\"2\\\"></input>\\n\";\n"
                   " htmlString += \"<select id=\\\"active\" + alarmNo + \"\\\" name=\\\"active\" + alarmNo + \"\\\">\\n\";\n"
                   " htmlString +=\"<option value=1>Enabled</option>\\n\";\n"
                   " htmlString +=\"<option value=0>Disabled</option>\\n\";\n"
                   " htmlString +=\"<input type=\\\"hidden\\\" id=\\\"delete\" + alarmNo + \"\\\"  name=\\\"delete\" + alarmNo + \"\\\" value=false/>\\n\";\n"
                   " htmlString +=\"</select>\\n\";\n"
                   " htmlString +=\"<a href=\\\"#\\\" onclick=\\\"javascript:deleteAlarm(\" + alarmNo + \")\\\">Delete</a>\\n\";\n"
                   " htmlString +=\"</div><br>\";\n"
                   " return htmlString;\n"
                   "}"
                   "</script>"
                   "</body>"
                   "</html>";


//SSD1306  display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);

void setup() {
  Serial.begin(115200);
  mydisp.begin();
  // mydisp.print("uploading start screen now...(1024 bytes)");
  mydisp.setI2CAddress(0x29);  //this function only working when you connect using I2C, from 1 to 127
  mydisp.setRotation(3);
  delay(500);
  SPIFFS.begin();
  // display.init();
  // display.clear();
  pinMode(PIR_PIN, INPUT);
  pinMode(PIN_BUSY, INPUT);
  wifiManager.autoConnect("WeatherStationAP");
  //mydisp.drawStr(0, 0, "Setting up Wifi.....");
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }
  //mydisp.drawStr(0, 1, "Updating Time Client....");
  updateTimeClient();

  postThingSpeak(0);
  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }
  Serial.println("Checking alarms array...");
  //mydisp.clearScreen();
  //mydisp.drawStr(0, 2, "  Reading File...");

  readFile();
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    Serial.println("Serving up HTML...");
    server.send(200, "text/html", html);
    Serial.println("Done serving up HTML...");
  });
  server.on("/data", HTTP_GET, []() {
    Serial.printf("get heap size start: %u\n", ESP.getFreeHeap());
    DynamicJsonBuffer jsonBuffer;
    JsonArray& array = jsonBuffer.createArray();
    for (int i = 0; i < alarms.size(); i++) {
      alarm* n = alarms.get(i);
      JsonObject& nested = array.createNestedObject();
      nested["day"] = n->alarmDay;
      nested["hour"] = n->alarmHour;
      nested["minute"] = n->alarmMinute;
      nested["enabled"] = n->alarmEnabled;
    }
    String html = "";
    array.printTo(html);
    server.send(200, "application/json", html);
    Serial.printf("get heap size end: %u\n", ESP.getFreeHeap());
  });
  server.on("/update", HTTP_POST, []() {
    Serial.printf("post heap size start: %u\n", ESP.getFreeHeap());
    HTTPUpload& upload = server.upload();
    String noAlarmsStr = server.arg("noAlarms");
    Serial.println(noAlarmsStr);
    int noAlarms = noAlarmsStr.toInt();
    Serial.printf("No Alarms: %i\n", noAlarms);
    alarms = LinkedList<alarm*>();
    for (int i = 0; i < noAlarms; i++) {
      if (server.arg("delete"  + String(i)).toInt() == 0) {
        alarm* n = new alarm();
        n->alarmDay = server.arg("day" + String(i)).toInt();
        n->alarmHour = server.arg("hour" + String(i)).toInt();;
        n->alarmMinute = server.arg("minute" + String(i)).toInt();;
        n->alarmEnabled = server.arg("active" + String(i)).toInt();;
        alarms.add(n);
        Serial.printf("Alarm %i %i:%i %i\n", n->alarmDay, n->alarmHour, n->alarmMinute, n->alarmEnabled);
      }
    }
    updateFile(alarms);
    server.sendHeader("Connection", "close");
    server.sendHeader("Access - Control - Allow - Origin", "*");
    server.send(200, "text / plain", "OK");
    Serial.printf("post heap size end: % u\n", ESP.getFreeHeap());
  });
  server.begin();
  MDNS.addService("http", "tcp", 80);
  mp3Serial.begin (9600);
 // mydisp.drawStr(0, 4, "Setting up mp3 player");
  mp3_set_serial (mp3Serial);
  // Delay is required before accessing player. From my experience it's ~1 sec
  delay(1000);
  mp3_set_volume (20);
}

void updateTimeClient() {
  timeClient.setupByIP();
  ntpClient = timeClient.getNTPClient();
  updateNtpClient(ntpClient);
}

void updateFile(LinkedList<alarm*> alarms) {
  //  Serial.printf("update file settings heap size: %u\n", ESP.getFreeHeap());
  if (SPIFFS.exists("alarms.txt")) {
    if (SPIFFS.exists("alarms.old")) {
      SPIFFS.remove("alarms.old");
    }
    SPIFFS.rename("alarms.txt", "alarms.old");
    SPIFFS.remove("alarms.txt");
  }
  File f = SPIFFS.open("alarms.txt", "w");
  if (!f) {
    Serial.println("file open failed");
  } else {
    for (int i = 0; i < alarms.size(); i++) {
      alarm* n = alarms.get(i);
      f.printf("%i,%i,%i,%i\n", n->alarmDay, n->alarmHour, n->alarmMinute, n->alarmEnabled);
      Serial.printf("Wrote: %i,%i,%i,%i\n", n->alarmDay, n->alarmHour, n->alarmMinute, n->alarmEnabled);
    }
    f.close();
    yield();
  }
  //  Serial.printf("update file settings heap size end: %u\n", ESP.getFreeHeap());
  setTodaysAlarms();
}

void readFile() {
  //  Serial.printf("read file heap size start: %u\n", ESP.getFreeHeap());
  File f = SPIFFS.open("alarms.txt", "r");
  if (!f) {
    Serial.println("file open failed");
  }  Serial.println("====== Reading from SPIFFS file =======");
  bool stillReading = true;
  while (stillReading) {
    String s = f.readStringUntil('\n');
    if (s != "") {
      //     Serial.println(s);
      int commaIndex = s.indexOf(',');
      int secondCommaIndex = s.indexOf(',', commaIndex + 1);
      int thirdCommaIndex = s.indexOf(',', secondCommaIndex + 1);
      alarm* n = new alarm();
      n->alarmDay = s.substring(0, commaIndex).toInt();
      n->alarmHour = s.substring(commaIndex + 1, secondCommaIndex).toInt();
      n->alarmMinute = s.substring(secondCommaIndex + 1, thirdCommaIndex).toInt(); //To the end of the string
      n->alarmEnabled = s.substring(thirdCommaIndex + 1).toInt();
      Serial.printf("Alarm: %i %i:%i %i\n", n->alarmDay, n->alarmHour, n->alarmMinute, n->alarmEnabled);
      alarms.add(n);
    } else {
      Serial.println("Done Reading....");
      stillReading = false;
    }
  }
  f.close();
  //  Serial.printf("red file heap size end: %u\n", ESP.getFreeHeap());
  setTodaysAlarms();

}

void updateNtpClient(NTPClient* ntpClient) {
  timeClient.updateTime();
  setTime(ntpClient->getEpochTime());
  while (year() == 1970) {
    Serial.println("Updating time....");
    timeClient.updateTime();
    yield();
    setTime(ntpClient->getEpochTime());
  }
}

void loop() {
  //  Serial.printf("loop heap size start: %u\n", ESP.getFreeHeap());
  if (hour() == 3 && !offset) {
    offset = true;
    updateTimeClient();
    Serial.println("Updating time....");
    updateNtpClient(ntpClient);
    yield();
    postThingSpeak(2);
  }
  yield();
  if (savedDayOfWeek != weekday()) {
    offset = false;
    postThingSpeak(1);
    setTodaysAlarms();
    yield();
    Serial.printf("Current time %s \n", timeClient.getFormattedTime().c_str());
    savedDayOfWeek = weekday();
    wunderground.updateForecast(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
    yield();
  }
  showTime();
  for (int i = 0; i < alarmsToday.size(); i++) {
    alarm* n = alarmsToday.get(i);
    if (n->alarmHour == hour()) {
      if (n->alarmMinute == minute()) {
        pirState = digitalRead(PIR_PIN) ;
        mp3_next();
        delay(200);
        busy = digitalRead(PIN_BUSY);
        Serial.print("Motion: ");
        Serial.print(pirState);
        Serial.print(" Busy: ");
        Serial.println(busy);
        int repeatCount = 0;
        while (n->alarmMinute == minute() && motion == false && n->alarmEnabled) {
          Serial.println("Alarm!");
          mp3_next();
          busy = digitalRead(PIN_BUSY);
          Serial.print("Motion: ");
          Serial.print(pirState);
          Serial.print(" Busy: ");
          Serial.println(digitalRead(PIR_PIN));
          if (digitalRead(PIR_PIN) != pirState) {
            Serial.println("motion detected...");
            if (repeatCount++ > 2) {
              mp3_stop();
              motion = true;
            }
          }
          else {
            Serial.println("Playing....");
            mp3_play();
          }
          delay(500);
          showTime();
        }
        alarmsToday.remove(i);
      }
      motion = false;
    }
  }
  startMinute = minute();
  while (minute() == startMinute) {
    server.handleClient();
  }
}

String padInt(int value, int retLength) {
  String padding = "00";
  String valueString = String(value);
  if (valueString.length() < retLength) {
    valueString = padding.substring(0, retLength - valueString.length()) + valueString;
  }
  return valueString;
}

void showTime() {
    Serial.println("Hour: " + hour());
    if (hour() > 20 || hour() < 8) {
      mydisp.backLightBrightness(66);
    } else {
      mydisp.backLightBrightness(90);
    }
    mydisp.setFont(200);
    mydisp.setPrintPos(1, 0);
    if (hour() > 12) {
      mydisp.print(String(hour() - 12) + ":" + padInt(minute(), 2) );
    } else {
      mydisp.print(String(hour()) + ":" + padInt(minute(), 2) );

    }
    mydisp.setFont(120);
    mydisp.setPrintPos(24, 0);
    if (hour() > 12) {
      mydisp.print("PM");
    } else {
      mydisp.print("AM");
    }
    //mydisp.print(timeClient.getFormattedTime().substring(0, 5));
    mydisp.setFont(201);

    String day = wunderground.getForecastTitle(0).substring(0, 3);
    day.toUpperCase();
    mydisp.setPrintPos(0, 4);
    mydisp.print(day);
    mydisp.setPrintPos(0, 5);
    mydisp.print( wunderground.getForecastLowTemp(0) + "|" + wunderground.getForecastHighTemp(0));
    displayWeatherImage(0, 130, wunderground.getForecastIcon(0));
    day = wunderground.getForecastTitle(2).substring(0, 3);
    day.toUpperCase();
    mydisp.setPrintPos(7, 4);
    mydisp.print(day);
    mydisp.setPrintPos(7, 5);
    mydisp.print( wunderground.getForecastLowTemp(2) + "|" + wunderground.getForecastHighTemp(2));
    displayWeatherImage(120, 130, wunderground.getForecastIcon(2));
    day = wunderground.getForecastTitle(4).substring(0, 3);
    day.toUpperCase();
    mydisp.setPrintPos(14, 4);
    mydisp.print(day);
    mydisp.setPrintPos(14, 5);
    mydisp.print( wunderground.getForecastLowTemp(4) + "|" + wunderground.getForecastHighTemp(4));
    displayWeatherImage(220, 130, wunderground.getForecastIcon(4));
    yield();
    mydisp.setFont(10);
    mydisp.setPrintPos(40, 24);
    mydisp.print(WiFi.localIP().toString());
}

void postThingSpeak(int postValue) {
  if (client.connect(thingSpeakserver, 80)) { // "184.106.153.149" or api.thingspeak.com
    String postStr = apiKey;
    postStr += "&field1=";
    postStr += String(postValue);
    postStr += "\r\n\r\n";

    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);

  }
}

void setTodaysAlarms() {
  Serial.print("Day of week: ");
  Serial.println(weekday());
  Serial.print("Month: ");
  Serial.println(month());
  Serial.print("Day: ");
  Serial.println(day());
  Serial.print("Year: ");
  Serial.println(year());
  alarmsToday = LinkedList<alarm*>();
  for (int i = 0; i < alarms.size(); i++) {
    alarm* n = alarms.get(i);
    if (n->alarmDay ==  weekday()) {
      alarmsToday.add(n);
    }
  }
  Serial.print("Alarms today: ");
  Serial.println(alarmsToday.size());
}

void displayWeatherImage(int x, int y, String iconText) {
  if (iconText == "Q") mydisp.drawBitmap256(x, y, 50, 50, chancerain);
  if (iconText == "V") mydisp.drawBitmap256(0, 110, 40, 40, snow);
  if (iconText == "S") mydisp.drawBitmap256(x, y, 50, 50, tstorms);
  if (iconText == "B") mydisp.drawBitmap256(x, y, 50, 50, sunny);
  if (iconText == "F") mydisp.drawBitmap256(0, 110, 40, 40, snow);
  if (iconText == "M") mydisp.drawBitmap256(x, y, 50, 50, foggy);
  if (iconText == "E") mydisp.drawBitmap256(x, y, 50, 50, foggy);
  if (iconText == "Y") mydisp.drawBitmap256(x, y, 50, 50, cloudy);
  if (iconText == "H") mydisp.drawBitmap256(x, y, 50, 50, partlycloudy);
  if (iconText == "J") mydisp.drawBitmap256(x, y, 50, 50, partlycloudy);
  if (iconText == "W") mydisp.drawBitmap256(x, y, 50, 50, sleet);
  if (iconText == "R") mydisp.drawBitmap256(x, y, 50, 50, chancerain);
  if (iconText == "0") mydisp.drawBitmap256(x, y, 50, 50, tstorms);
}

