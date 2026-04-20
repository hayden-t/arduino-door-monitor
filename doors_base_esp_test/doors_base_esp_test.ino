void setup() {
 
    Serial.begin(115200);
    Serial3.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.println("Mega Online");

   digitalWrite(LED_BUILTIN, HIGH);  
   delay(250);                      
   digitalWrite(LED_BUILTIN, LOW);   
   delay(250);  
}

void loop() {
 
  while (Serial.available()) {
     digitalWrite(LED_BUILTIN, HIGH);
     
     String message = Serial.readString();
     Serial.print("Recieved: ");
     
     Serial.println(message);
     Serial3.println(message);
      
     digitalWrite(LED_BUILTIN, LOW);
  }
}
