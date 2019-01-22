#include <SPI.h>
#include <RF24.h>
#include <RF24Network.h>
#include "printf.h"
#include "doors.h"
#include "general.h"


//#define TEST_MODE
#define DUMMY_CAST

#define RELAY01
//#define RELAY011
//#define RELAY021
//#define RELAY0111
//#define RELAY01111

#include "pins.h"

//#define LOW_PA
#define MED_PA

int TRANSMIT_LIMIT = 20;//only allow transmit event for each room every x seconds

// nRF24L01(+) radio attached using Getting Started board
RF24 radio(9, 10);
// Network uses that radio
RF24Network network(radio);
int channel = 84;
const uint16_t root = 00;

byte currentPin = 0;

#ifndef DUMMY_CAST
cast casts[maxCast];//used to broadcast
#else
//dummy cast data
cast casts[maxCast] = {
  {10,1255},
  {11,1335},
  {34,935},

};
#endif

void setup(void)
{
    Serial.begin(9600);
  //  printf_begin();   
       
       
   pinMode(ledPin, OUTPUT);
   delay(1000);
  //setup door pins 
    for(int i=0; i < numPins; i++){
        pinMode(pins[i].pin, INPUT_PULLUP);
      
        digitalWrite(ledPin, HIGH);
        delay(50);
        digitalWrite(ledPin, LOW);
        delay(50);
      
    }
    
    SPI.begin();
  
    radio.begin();
    network.begin(channel, this_node);
    #ifdef LOW_PA 
      radio.setPALevel(RF24_PA_LOW);//-12dbm
    #endif  
    #ifdef MED_PA
      radio.setPALevel(RF24_PA_HIGH);
    #endif
    radio.setDataRate(RF24_250KBPS);  
  
  Serial.print("[Radio] Startup Address:");   
  char address[5];
  sprintf(address, "0%o", this_node);
  Serial.print(address);
  Serial.print(" CH:");
  Serial.print(channel);
  Serial.print(" PA:");
  Serial.print(radio.getPALevel());
  Serial.print(" DR:");
  Serial.println(radio.getDataRate());  

  
   #ifdef TEST_MODE
      test();
   #endif

   
}


unsigned long castTimer;
int sleep_count = 0;

void loop(void)
{
  

  if(castTimer > millis())castTimer = millis();//check for overflow
     

     network.update();
  
    // Is there anything ready for us?
    while ( network.available())
    {
          RF24NetworkHeader header;
          network.peek(header);
          
       if(header.type == 'B'){//broadcast data recieved
          
            network.read(header,&casts,sizeof(casts));
            Serial.println("[Cast] Recieve ");
            sendEvents(true);
            
       }else if(header.type == 'T'){//test request
             Serial.print("[Test] Sending Reply, ");
             network.read(header,0,0);
             RF24NetworkHeader response_header( header.from_node, 'Z');
             boolean ok = network.write(response_header,NULL,0);
             if (ok)
                    Serial.println("Ok");
              else{
                    Serial.println("Failed");
              }           
      }else if(header.type == 'R'){//transmit success reply
              
              byte id;
              network.read(header, &id, sizeof(id));
              
              Serial.print("[Radio] Success: ");
              Serial.println(id);
              
              for(int i=0; i < numPins; i++){
                if(pins[i].id == id)pins[i].queue = false;
              }
                     
       }else{           
            Serial.println("[Radio] Unknown Header ");
            network.read(header,0,0);           
       }
    }
    
    sendEvents(false);
    checkDoors(); 
    transmit();


}//end loop

void checkDoors(void){
  
  //0 == no motion, relay closed to gnd
  //1 == motion or not connected, pullups
  
  for(int i=0; i < numPins; i++){
    
    int state = digitalRead(pins[i].pin);//read pin state
    
    if(state == 1 && pins[i].state == 0){//check for change
         //has changed, transmit
         pins[i].state = 1;         
         
         if(pins[i].last > millis())pins[i].last = millis();//check for overflow
         
         if(pins[i].last && millis() - pins[i].last < (TRANSMIT_LIMIT * 1000))return;//transmit interval limit
         
         pins[i].last = millis();
         
         Serial.print("[Door] Motion in Room ");
         Serial.println(doors[pins[i].id].name);
         
         currentPin = i;
         
         pins[i].queue = true;
         
    }else if(state == 0 && pins[i].state == 1){//check for revert
        //has stopped, reset
         pins[i].state = 0;
    }
    
  }
  
}
void sendEvents(boolean start){
  
  static int attempt = 0;
  
#ifdef DUMMY_CAST
  static boolean casting = true;
#else
  static boolean casting = false;
#endif
  
  if(start){   
    attempt = 0;
    casting = true;
    return;
  }
  
  if(!casting || millis() - castTimer < 250)return;            
           
           radio.stopListening();
           radio.openWritingPipe(pipes[0]);           
           radio.setAutoAck(false); 

           bool ok = radio.write( &casts, sizeof(casts), true);                        
          
           Serial.print("[Cast] Send ");
           Serial.println(attempt);           
          // Serial.println(ok);
          
            
            radio.startListening();
            radio.setAutoAck(true);
            
            attempt++;
            if(attempt >= 14){
              attempt = 0;
              casting = false;
              Serial.println("[Cast] Done ");
             // Serial.println();
            }
            castTimer = millis();
}



void transmit() {

    static unsigned long lastAttempt = 0;    
            
           if(pins[currentPin].queue){
             
               if(millis() - lastAttempt > 1000){
                 
                 Serial.print("[Radio] Transmit: ");
                 Serial.print(doors[pins[currentPin].id].name);
                 digitalWrite(ledPin, HIGH);
            
                  RF24NetworkHeader header(root, 'D');
                  bool ok = network.write(header, &pins[currentPin].id, sizeof(pins[currentPin].id));
                  if (ok)
                    Serial.println(" . ");
                  else
                    Serial.println(" x ");
            
                  lastAttempt = millis();
                  currentPin++;
                  
                 }
        
            }else currentPin++;
            
            if(currentPin >= numPins)currentPin=0;
            if(millis() - lastAttempt > 1000)digitalWrite(ledPin, LOW);

}

void test(void) {

            static uint16_t target = this_node;
            unsigned long wait;            
            
            start:
            
            int i = 0;
            
            if(target == root){
                Serial.println("[Test] Finished ");

                 delay(5000);
                 target = this_node;
                 goto start;
            }


            target = getParent(target);            
            
            retry:
            
            wait = millis();
            i++;
            Serial.print("[Test] Sending "); 

            char address[5];
            sprintf(address, "0%o", target);
            Serial.print(address);
                  
             RF24NetworkHeader response_header( target, 'T');
             boolean ok = network.write(response_header,NULL,0);
              if (ok)
                Serial.print(" . ");
              else
                Serial.print(" x "); 
               
               while(1){
                 
                 if(i >= 5){
                    Serial.println();
                    Serial.println("[Test] Timeout.");
                    break;//300 sec (5 min)                    
                  }
                 
                 if(millis() - wait > 1000){
                  Serial.println();
                    //Serial.println("[Test] Try Again.");
                    goto retry;
                  }              

      
                    network.update();
                    
                    while ( network.available() )//return message recieved
                    {
                        RF24NetworkHeader header;     
                        network.peek(header);  
                     if(header.type == 'Z'){
                        network.read(header, NULL, 0);
                        Serial.println("Success");
                        delay(500);
                        goto start;
                      }else{
                        //Serial.print("Unknown Packet");
                        network.read(header, NULL, 0);
                      }
                    }
                    
               }           

}

uint16_t getParent(uint16_t target){
  
  if(target == 00 || target == root)return root;
    
  uint16_t node_mask_check = 0xFFFF;
  while ( target & node_mask_check )
    node_mask_check <<= 3;
  
  uint16_t node_mask = ~ node_mask_check;  
  uint16_t parent_mask = node_mask >> 3;  
  uint16_t parent_node = target & parent_mask;
  
  return parent_node; 
  
}

