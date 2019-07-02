#include <SoftwareSerial.h>

#define SSID "redmi"
#define PASS "12345678"
#define EMONCMS_IP "80.243.190.58"         // emoncms.org
#define THINGSPEAK_IP "184.106.153.149"    // thingspeak.com
#define MAX_FAILS 3

SoftwareSerial espSerial(10, 11);          // RX, TX

#include <DHT.h>
DHT dht(3, DHT11);

unsigned long sample_interval = 5000;
unsigned long last_time;
int fails = 0;

void setup() {
  pinMode(10, INPUT);    // softserial RX - yellow
  pinMode(11, OUTPUT);   // softserial TX - blue
  pinMode(9, OUTPUT);    // esp reset - white
  pinMode(3, INPUT);     // DHT - ...
 
  delay(3000);           // to enable uploading??
 
  // Same speed, ESP softserial cannot be faster than 19200
  Serial.begin(9600);    
  espSerial.begin(9600);
  espSerial.setTimeout(2000);
 
  dht.begin();

  last_time = millis();  // get the current time;
 
  Serial.print(F("send DHT11 sensor data to ThingSpeak.com using ESP8266 "));
  Serial.print(F("every "));
  Serial.print(sample_interval/1000);
  Serial.println(F("s."));
  
  if (!resetESP()) return;
  if (!connectWiFi()) return;
}

void echoAll() {
  while (espSerial.available()) {
    char c = espSerial.read();
    Serial.write(c);
    if(c=='\r') Serial.print('\n');
  }
}

boolean resetESP() {
   // - test if module ready
   Serial.print(F("reset ESP8266..."));
 
   // physically reset EPS module
   digitalWrite(9, LOW);          
   delay(100);
   digitalWrite(9, HIGH);  
   delay(500);
   
   if (!send("AT+RST", "ready", F("%% module no response"))) return false;
   
   Serial.print(F("module ready..."));
   return true;
}

boolean connectWiFi() {
  int tries = 5;
  
  while(tries-- > 0 && !tryConnectWiFi());
  
  if (tries <= 0) {
    Serial.println(F("%% tried X times to connect, please reset"));
    return false;
  }
  
  delay(500); // TOOD: send and wait for correct response?
  
  // set the single connection mode
  espSerial.println("AT+CIPMUX=0");
  
  delay(500); // TOOD: send and wait for correct response?
  // TODO: listen?
  
  return true;
}

boolean tryConnectWiFi() {
   espSerial.println("AT+CWMODE=1");
   delay(2000); // TOOD: send and wait for correct response?
   
   String cmd="AT+CWJAP=\"";
   cmd+=SSID;
   cmd+="\",\"";
   cmd+=PASS;
   cmd+="\"";
   
   if (!send(cmd, "OK", F("%% cannot connect to wifi..."))) return false;
   
   Serial.println(F("WiFi OK..."));
   return true;
}

boolean send(String cmd, char* waitFor, String errMsg) {
    espSerial.println(cmd);
    if (!espSerial.find(waitFor)) {
      Serial.print(errMsg);
      return false;
    }
    return true;
}

boolean connect(char* ip) {
   String cmd; 
   cmd = "AT+CIPSTART=\"TCP\",\"";
   cmd += ip;
   cmd += "\",80";
   espSerial.println(cmd);
   // TODO: this is strange, we wait for ERROR
   // so normal is to timeout (2s) then continue
   if(espSerial.find("Error")) return false;
   return true;
}

boolean sendGET(String path) {
   String cmd = "GET ";
   cmd += path;
   
   // Part 1: send info about data to send
   String xx = "AT+CIPSEND=";
   xx += cmd.length();
   if (!send(xx, ">", F("%% connect timeout"))) return false;
   Serial.print(">");
   
   // Part 2: send actual data
   if (!send(cmd, "SEND OK", F("%% no response"))) return false;
   
   return true;
}

void loop() {
  if (millis()-last_time < sample_interval) return;
  
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  last_time = millis();

  Serial.print(last_time);
  Serial.print(": ");
  Serial.print(t, 2);
  Serial.print("C");
  Serial.print(" ");
  Serial.print(h, 2);
  Serial.print("% ");
        
  if (!sendDataThingSpeak(t,h)) {
    Serial.println(F("%% failed sending data"));
    // we failed X times, at MAX_FAILS reconnect till it works
    if (fails++ > MAX_FAILS) {
      if (!resetESP()) return;
      if (!connectWiFi()) return;
    }
  } else {
    fails = 0;
  }
  
  Serial.println();
}

boolean sendDataThingSpeak(float temp, float hum) {
   if (!connect(THINGSPEAK_IP)) return false;

   String path = "/update?key=DYZ18BVX0FIESTHU&field1=0";
   path += temp;
   path += "&field2=";
   path += hum;
   path += "\r\n";
   if (!sendGET(path)) return false;
   
   Serial.print(F(" thingspeak.com OK"));
   return true;
}
