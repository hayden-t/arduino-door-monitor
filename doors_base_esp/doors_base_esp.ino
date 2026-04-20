#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

/*
 * {"type": "settings","ssid": "WIFI_NAME","password": "","targets":["192.168.1.38"]}
 * {"type": "doors","door": "Door1"}
 * 
 */


WiFiUDP udp;

unsigned int port = 5005;

// List of target devices
IPAddress targets[10];
int numTargets = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.println("");
  Serial.println("Awaiting Settings...");
  udp.begin(port);  
      
}

void loop() {

  while (Serial.available()) {
     digitalWrite(LED_BUILTIN, HIGH);
     
     String message_string = Serial.readString();
     Serial.print("Recieved: ");
     Serial.println(message_string);

     JsonDocument message_json;
     deserializeJson(message_json, message_string);

    String message_type = message_json["type"];

    if (message_type == "settings"){

              Serial.println("Settings Recieved");
       
              String ssid = message_json["ssid"];
              String password = message_json["password"];

              JsonArray ipArray = message_json["targets"].as<JsonArray>();

             for (JsonVariant ip : ipArray) {
                   String ipString = ip.as<String>();
                    
                   int firstOctet = 0, secondOctet = 0, thirdOctet = 0, fourthOctet = 0;
                   sscanf(ipString.c_str(), "%d.%d.%d.%d", &firstOctet, &secondOctet, &thirdOctet, &fourthOctet);
                
                    // Create an IPAddress object using the octets
                   targets[numTargets] = IPAddress(firstOctet, secondOctet, thirdOctet, fourthOctet);
                  numTargets++;
                   if (numTargets >= 10) break;  // Prevent exceeding the array size
                }

                  WiFi.begin(ssid, password);
                  while (WiFi.status() != WL_CONNECTED) {    
                    digitalWrite(LED_BUILTIN, HIGH);
                    delay(250);
                    Serial.print(".");
                    digitalWrite(LED_BUILTIN, LOW);
                    delay(250);
                  }
                  Serial.println("Wifi Connected");
                
                          
    }else if (message_type == "doors") {
       
       if(WiFi.status() == WL_CONNECTED){
        
              Serial.println("Door Recieved");
              
              String door = message_json["door"];                      
              for (int i = 0; i < numTargets; i++) {
                udp.beginPacket(targets[i], port);
                udp.write(door.c_str(), door.length());
                udp.endPacket();
              }
       }
       
    }else{
       Serial.println("Unknown Recieved");
    }

      digitalWrite(LED_BUILTIN, LOW);
  }


}
