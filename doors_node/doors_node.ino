#include <SPI.h>
#include <RF24.h>
#include <RF24Network.h>
#include "LowPower.h"
#include "printf.h"


//room door id
int id = 35;
// Address of our node
const uint16_t this_node = 03;

boolean relay = true;

boolean test_relay = false;
#define LOOP_TEST

int sleep = 1;//time before sleep (minutes)
int sleep_times = 225;//times to sleep between pings//30mins
//time between pings = 8s/4s * sleep_times + sleep 

//#define LOW_PA
#define MED_PA

const uint64_t pipes[6] = { 0xf04b962d3cLL, 0xf04b962d5aLL, 0xf04b962d69LL, 0xf04b962d96LL, 0xf04b962da5LL, 0xf04b962dc3LL };
//for mobile

unsigned long last = 0;//last change

byte state;//state

int bounce = 1 * 1000;//door debounce
boolean queue = true;//transmit on startup

// nRF24L01(+) radio attached using Getting Started board
RF24 radio(9, 10);

// Network uses that radio
RF24Network network(radio);
int channel = 10;
const uint16_t root = 01;

typedef struct{
  byte id;
  byte status;
  boolean change;
  byte voltage;
} node;

typedef struct {
  byte id;
  byte hours;
  byte mins;
} cast;

const int maxCast = 5;

cast casts[maxCast];//used to broadcast
/*
//dummy cast data
cast casts[maxCast] = {
  {10,1255},
  {11,1335},
  {34,935},

};
*/

char* status[] = {
       "CLOSED ",
       "OPENED ",
       "MISSING",
       "BATTERY"
 };

boolean AWAKE;

void setup(void)
{
  Serial.begin(9600);
  //printf_begin();

  pinMode(5, OUTPUT);//radio on
  digitalWrite(5, LOW);//radio on

  SPI.begin();
  radioStart();
  
  Serial.print("[Radio Startup] Address:");   
  char address[5];
  sprintf(address, "0%o", this_node);
  Serial.print(address);
  Serial.print(" CH:");
  Serial.print(channel);
  Serial.print(" PA:");
  Serial.print(radio.getPALevel());
  Serial.print(" DR:");
  Serial.println(radio.getDataRate());
  
   if(!relay){
      Serial.print("[Node Startup] id:");
      Serial.print(id);
      Serial.print(" Awake:");
      Serial.print(sleep);
      Serial.print(" Sleep:");
      Serial.print(sleep_times*8/60);
      int voltage = readVcc();
      Serial.print(" Volts: ");
      Serial.print( (float)voltage/10, 1 );
      Serial.print("v FreeMem: ");
      Serial.println(freeRam());
      
      pinMode(2, INPUT_PULLUP);//reed switch
      attachInterrupt(0, door, CHANGE);//reed switch  
      delay(1000);
      state = digitalRead(2);//initial state
    }
    else{
       Serial.println("[Relay Online]"); 
    }
   
    if(test_relay || !relay)test();
}


unsigned long castTimer;
int sleep_count = 0;

void loop(void)
{
  
  if(last > millis())last = millis();//check for overflow
  if(castTimer > millis())castTimer = millis();//check for overflow

  if (sleep_count >= sleep_times && !relay) {//ping stats at interval
    sleep_count = 0;
    queue = true;
    //Serial.print("[Ping] ");
  }

  if ((last + bounce) < millis() && queue && !relay) { //time to transmit
    radioStart();
    transmit();
    queue = false;
  }
  
if(AWAKE){
     // Pump the network regularly
     
if(relay){
     network.update();
  
    // Is there anything ready for us?
    while ( network.available())
    {
          RF24NetworkHeader header;
          network.peek(header);
          
       if(header.type == 'B'){
          
            network.read(header,&casts,sizeof(casts));
            Serial.println("[Cast Recieve]");
            sendEvents(true);
            
       }else if(header.type == 'T'){
             Serial.print("[Test]: Sending Reply, ");
             network.read(header,0,0);
             RF24NetworkHeader response_header( header.from_node, 'Z');
             boolean ok = network.write(response_header,NULL,0);
             if (ok)
                    Serial.println("Ok");
              else{
                    Serial.println("Failed");
              }           
      }else{
            Serial.println("Unknown Header");
            network.read(header,0,0);
            break;
       }
    }
    sendEvents(false);
     
}else{
    while( radio.available())
    {
 
       cast payload;         
        // Fetch the payload, and see if this was the last one.
       radio.read( &payload, sizeof(payload) );
 
        Serial.print("[Clear]");
        Serial.print(" Id: ");
        Serial.print(payload.id);
        Serial.print(" Time: ");
        Serial.print(payload.hours);
        Serial.print(":");
        Serial.print(payload.mins);     
        
        if(payload.id != id){
           Serial.println(" Not Mine");
           return;
        }

      // send it to base             
      RF24NetworkHeader header(root, 'C');
      bool ok = network.write(header, &payload, sizeof(payload));
      if (ok)
        Serial.println(" Ok");
      else
        Serial.println(" Failed");

    }
  
  } 
}//end awake

  if (((last + bounce + (sleep*60000)) < millis()) && !relay) { //time for sleep

    Serial.println("[Sleep]");    
    AWAKE = false;
    digitalWrite(5, HIGH);//turn off radio
    delay(100);
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
 
    //code resume from here after interrupt
    sleep_count++;

  }
  
  //sendEvents();delay(5000);  

}//end loop

void sendEvents(boolean start){
  
  static int attempt = 0;
  static boolean casting = false;
  
  if(start){
    Serial.print("[Cast Send] ");
    attempt = 0;
    casting = true;
    return;
  }
  
  if(!casting || millis() - castTimer < 100)return;            
           
           radio.stopListening();
           radio.openWritingPipe(pipes[0]);           
           radio.setAutoAck(false); 

           bool ok = radio.write( &casts, sizeof(casts), true);                        
          
           Serial.print(ok);
           Serial.print(", ");
            
            radio.startListening();
            radio.setAutoAck(true);
            
            attempt++;
            if(attempt >= 14){
              attempt = 0;
              casting = false;
              Serial.println();
            }
            castTimer = millis();
}

void radioStart(void) {

    digitalWrite(5, LOW);  
    delay(100);
    AWAKE = true;
    radio.begin();
    network.begin(channel, this_node);
    #ifdef LOW_PA 
      radio.setPALevel(RF24_PA_LOW);//-12dbm
    #endif  
    #ifdef MED_PA
      radio.setPALevel(RF24_PA_HIGH);
    #endif
    radio.setDataRate(RF24_250KBPS);  
    radio.printDetails(); 
  
   if(!relay){//for mobile units
     radio.openReadingPipe(1, pipes[1]);
     radio.openReadingPipe(2, pipes[2]);
     radio.openReadingPipe(3, pipes[3]);
     radio.openReadingPipe(4, pipes[4]);
     radio.openReadingPipe(5, pipes[5]);
   }

}


void door(void) {

  last = millis();//start timer
  queue = true;
  sleep_count = 0;
  //cannot serial in interrupt
}



void transmit(void) {

  byte reading = digitalRead(2);
  boolean change = (state != reading ? true:false);
  state = reading;  
  byte voltage = readVcc();
  
  Serial.print("[Door] ");
  Serial.print("Id: ");
  Serial.print(id);
  Serial.print(" Door: ");
  Serial.print(status[state]);
  Serial.print(" Change: ");
  Serial.print(change);  
  Serial.print(" Volts: ");
  Serial.print((float)voltage/10,1);
  Serial.print("v");
  Serial.print(" Transmit: ");

    unsigned long lastAttempt = 0;
    int attempts = 0;

    while(1){
      
          network.update();
          
          while ( network.available() )//return message recieved
          {
              RF24NetworkHeader header;     
              network.peek(header);  
           if(header.type == 'R'){
              network.read(header, NULL, 0);
              Serial.println("Success");
              return;
            }
          }
          
          if(attempts >= 20){
            Serial.println("Giving Up");
            return;      
          }
          
          if(millis() - lastAttempt > 1000){//attempt connection
          
              node payload = { id, state, change, voltage};
              RF24NetworkHeader header(root, 'D');
              bool ok = network.write(header, &payload, sizeof(payload));
              if (ok)
                Serial.print(". ");
              else
                Serial.print("x ");
        
              lastAttempt = millis();
              attempts++;
          }
          
      }      

}

void test(void) {

            static uint16_t target = this_node;
            unsigned long wait;            
            
            start:
            
            int i = 0;
            
            if(target == root){
                Serial.println("[Test] Finished.");
               #ifdef LOOP_TEST
                 delay(1000);
                 target = this_node;
                 goto start;
               #endif
                return;
            }


            target = getParent(target);            
            
            retry:
            
            wait = millis();
            i++;
            Serial.print("[Test] Sending: "); 

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

byte readVcc() {
  long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA, ADSC));
  result = ADCL;
  result |= ADCH << 8;
  result = 1126400L / result; // Back-calculate AVcc in mV  
  return result/100;
}
int freeRam ()
{
    extern int __heap_start, *__brkval;
    int v;
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
