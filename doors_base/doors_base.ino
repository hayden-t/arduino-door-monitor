#include "T6963.h"
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <SdFat.h>
#include <Button.h>
#include "doors.h"
#include "general.h"

#define CAST
#define LCD
#define SD

//#define LOW_PA
#define MED_PA

boolean DEBUG = false;

int HOME_TIMEOUT = 30;//seconds

int CAST_EVERY = 10;//seconds between casts;
int CAST_DELAY = 1;//seconds delay between relays;
//maximum total delay between individual relay = CAST_DELAY + CAST_EVERY

const int ALARM_GAP = 3;//seconds ?gap between beeps groups
const int ALARM_COUNT = 3;//seconds ?number of beeps

int LOG_INTERVAL = 60;//only process motion event per x seconds per room


int channel = 84;
const uint16_t root = 00;


const int mobileRelaysCount = 5;
const uint16_t mobileRelays[mobileRelaysCount] = {01, 021, 011, 0111, 01111};

#ifdef SD
//init SD
const uint8_t chipSelect = 8;
SdFat sd;
#endif

#ifdef LCD
//init LCD
T6963 lcd(240,128,6,32);// 240x64 Pixel and 6x8 Font
const int lcdLines = 16;
const int lcdCols = 40;//change printLine() too
char line[lcdCols];
#endif

//init clock
RTC_DS1307 RTC;

// nRF24L01(+) radio attached using Getting Started board 
RF24 radio(9,10);
// Network uses that radio
RF24Network network(radio);

void setup(void)
{
  Serial.begin(9600);
  
  Serial.print("[Base Startup] maxEvents:");
  Serial.print(maxEvents);
  Serial.print(" maxDoors:");
  Serial.print(maxDoors);
  Serial.print(" FreeMem: ");
  Serial.println(freeRam());
   
  Wire.begin();  
  RTC.begin();
  //RTC.adjust(DateTime(__DATE__, __TIME__).get());//36000 +10 hrs//uncomment and upload once to set clock, comment again and upload after
 
  SPI.begin();    
  
  radio.begin();
  network.begin(channel, root);
    #ifdef LOW_PA 
      radio.setPALevel(RF24_PA_LOW);//-12dbm
    #endif  
    #ifdef MED_PA
      radio.setPALevel(RF24_PA_HIGH);
    #endif
  radio.setDataRate(RF24_250KBPS);  

  Serial.print("[Radio Startup] Address:"); 
  char address[5];
  sprintf(address, "0%o", root);
  Serial.print(address); 
  Serial.print(" CH:");
  Serial.print(channel);
  Serial.print(" PA:");
  Serial.print(radio.getPALevel());
  Serial.print(" DR:");
  Serial.println(radio.getDataRate()); 
  
#ifdef LCD
  lcd.Initialize();
#endif
 
  pinMode(ledPin, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);  

  loadSettings();//from Eeprom  

  logEvent(0, 1);

#ifdef LCD
  lcd.TextGoTo(0,3);
  lcd.writeString("   WIRELESS DOOR MONITOR BASE STATION");
  lcd.TextGoTo(0,5);
  lcd.writeString("            www.httech.com.au"); 
  
 #ifdef CAST 
  lcd.TextGoTo(0,9);
  sprintf(line, "           Mobile Sync: %ds", CAST_EVERY);
  printLine(line); 
 #endif

#endif

tone(BUZZER_PIN, 3900, 300);
digitalWrite(ledPin, HIGH);
digitalWrite(ledPin, LOW);

#ifdef LCD
  delay(5000);
  lcd.clearText();
  if(exitb.isPressed()){
    Serial.println("Debug Activated");
    DEBUG = true;
    tone(BUZZER_PIN, 3900, 200);
    delay(200);
  }  
#endif
}
unsigned long loopTime = 0;
unsigned long buttonTimer = 0;
unsigned long castTimer = 0;
unsigned long castDelay = 0;

int second = 0;
boolean redraw = true;
int mode = 0;
int latestEvent = 0;
int viewEvent = 0;
int menuState = 0;
boolean openMenu = false;

int dst = 0;//daylight savings mode
int nill = dstb.stateChanged();//discard first read

int alarmsPlayed = ALARM_COUNT;

void loop(void)
{
  
  if(buttonTimer > millis())buttonTimer = millis();//check for overflow
  if(castTimer > millis())castTimer = millis();//check for overflow
  if(castDelay > millis())castDelay = millis();//check for overflow
  
  loopTime = millis();
  DateTime now = RTC.now();
  

  //dst button 
  if(dstb.stateChanged()){//not perfect, does not notice state change when off
      if(dstb.isPressed() && dst==1){
          Serial.println("dst off");
          RTC.adjust(now.get() - 3600);
          EEPROM.write(maxDoors+1, 0);
          dst = 0;
      }else if(dst==0){
          Serial.println("dst on");
          RTC.adjust(now.get() + 3600);
          EEPROM.write(maxDoors+1, 1);
          dst = 1;
      }
  }
  
  if(now.hour() >= 19 || now.hour() < 7)mode = 1;
  else mode = 2;
 // mode = 1;//all
   
  network.update();
  

  // Is there anything ready for us?
  while ( network.available() )
  {
    // If so, grab it and print it out
    RF24NetworkHeader header;
    network.peek(header);

   switch (header.type)
    {
    case 'D'://door
      {
          byte id;
          boolean alarmed;
          
          network.read(header,&id,sizeof(id));          
                  
         if(doors[id].stamp && now.get() - doors[id].stamp < LOG_INTERVAL){
           Serial.print("[Door] Discarding");//only report once per minute per door
           goto reply;
         }

        
         alarmed = ((doors[id].alarm && mode == 2) || (mode == 1) ? true : false );//check the mode   
          
         if(alarmed)alarmsPlayed = 0;
       
          createEvent(id, alarmed);
          
          doors[id].stamp = now.get();   
      
          Serial.print("[Door] ");
          Serial.print("Node: ");
          char address[5];
          sprintf(address, "0%o", header.from_node);
          Serial.print(address);
          Serial.print(" Room: ");
          Serial.print(doors[id].name);
          
          reply:
          
             Serial.print(" - Sending Reply: ");
             RF24NetworkHeader response_header( header.from_node, 'R');
             boolean ok = network.write(response_header,&id,sizeof(id));
             if (ok)
                    Serial.println("Ok");
              else{
                    Serial.println("Failed");        
              }
          
       break;
      }
    case 'C'://clear
      {
        
        Serial.print("[Clear]: ");
        cast payload;
        network.read(header,&payload,sizeof(payload));
      
        Serial.print("Id: ");
        Serial.print(payload.id);
        Serial.print(" Door: ");
        Serial.print(doors[payload.id].name);
        Serial.print(" Time: ");
        Serial.print(payload.hours);
        Serial.print(":");
        Serial.print(payload.mins);   
          
        for(int i = 0; i < maxEvents; i++){
          DateTime time (events[i].stamp);
          if( payload.id == events[i].doorId && payload.hours == time.hour() && payload.mins == time.minute() && events[i].alarmed == 1)
          {
                events[i].alarmed = 2;//cleared alarm
                Serial.println(" Found");
                sendEvents(true);
                logEvent(payload.id, 3);
                //break;//found it //find them all
          }
              
        }
        break;
      }
     case 'T'://Test Recieved
     {
             Serial.print("[Test]: Sending Reply, ");
             network.read(header,0,0);
             RF24NetworkHeader response_header( header.from_node, 'Z');
             boolean ok = network.write(response_header,NULL,0);
             if (ok)
                    Serial.println("Ok");
              else{
                    Serial.println("Failed");        
              }        
        break;
    }
    default:
    {
        Serial.println("Unknown Header");
        network.read(header,0,0);
        break;
    }

    };//end switch 
    
  }//end update packet 
  

  checkButtons();
  //checkAlarms();
  checkAlarm();
  //alarm(2);
  
  sendEvents(false);
  
  if(now.second() != second){
      redraw = true;
      second = now.second();
  }
#ifdef LCD  
  if(redraw){
    
    //header
    lcd.TextGoTo(0,0);     
    sprintf(line, "  Mode: %s   %02d:%02d:%02d   %02d-%02d-%02d", modes[mode],now.hour(), now.minute(), now.second(), now.day(), now.month(), now.year() );
    printLine(line);
    lcd.TextGoTo(0,1);
    lcd.writeString("----------------------------------------");
    //header
    
    if(openMenu)drawMenu();
    else drawEvents();
    redraw = false;
  }

  if(0){
    lcd.TextGoTo(0,0);
    sprintf(line, "%02d", millis() - loopTime);
    lcd.writeString(line);
    delay(500);
  }
#endif
}//end loop


void createEvent(byte id, boolean alarmed){


      DateTime now = RTC.now();
      unsigned long adjusted = now.get();
      if(!now.hour() && !now.minute())adjusted =+ 60;//add 1 minute
  
      if(latestEvent == maxEvents)latestEvent = 1;//buffer full, save elsewhere b4 wipe ?
      else latestEvent++;
      
      if(viewEvent != 1 && viewEvent < maxEvents )viewEvent++;//viewing history, increment viewed event
      
      strcpy(events[latestEvent].name, doors[id].name);
      events[latestEvent].stamp = adjusted;
      events[latestEvent].alarmed = (alarmed ? true : false);
      events[latestEvent].doorId = id;
      
      if(alarmed)sendEvents(true);//force cast update

      logEvent(latestEvent, 0);
      redraw = true;
}

void loadSettings(void){
  
  for(int i = 0; i < maxDoors; i++){       
    doors[i].alarm =  (EEPROM.read(i) == 1 ? 1 : 0);
  }
  mode = EEPROM.read(maxDoors);
  if(mode > 2)mode = 0;
  dst = (EEPROM.read(maxDoors+1) == 1 ? 1 : 0);

}

void checkAlarm(){
  
  static unsigned long alarmTimer = millis();
  static int note = 0;
  int noteGap = 150;
  static boolean pause = false;
  
  boolean alarm = false;
  
  
  if(alarmsPlayed < ALARM_COUNT)alarm = true;
  
  if(!alarm){
    pause = false;
    note = 0;
    digitalWrite(ledPin, LOW);
    return;
  }
  
  if(millis() % 1000 < 500)digitalWrite(ledPin, HIGH);
  else digitalWrite(ledPin, LOW);

  
  if(pause){
      if(millis() - alarmTimer > (ALARM_GAP*1000))pause = false;
      else return;
  }

  if(millis() - alarmTimer > noteGap*note){//gap between notes
      switch(note){
           case 0:
               tone(BUZZER_PIN, 3800, 75);
               note++;
               alarmTimer = millis();
             break;     
           case 1:     
               tone(BUZZER_PIN, 3900, 75);
               note++;
             break;
           case 2:     
               tone(BUZZER_PIN, 4000, 75);
               note++;
             break;
           case 3:     
               tone(BUZZER_PIN, 4100, 75);
               note++;     
             break;     
           case 4:
               note = 0;  
               pause = true;
               alarmsPlayed++;
             break;    
           default:
             break;
      }
     
  }

}


void checkButtons(void){
#ifdef LCD  
   //check buttons
 
//down button 
  if(down.uniquePress()){
                     if(!openMenu){
                        if(viewEvent < eventCount())viewEvent++;
                      }else{
                        if(menuState < maxDoors-1)menuState++;
                        else menuState = 0;
                      }   
                      buttonTimer = millis();
                      //Serial.println("Button1");
                      redraw = true;
  }
  
//up button  
   if(up.uniquePress()){
                     if(!openMenu){
                      if(viewEvent > 1)viewEvent--;
                    }else{
                      if(menuState > 0)menuState--;
                      else menuState = maxDoors-1;
                    }    
                      buttonTimer = millis();
                      //Serial.println("Button2");
                      redraw = true;
  }
  
//enter button 
  if(enter.uniquePress()){    
                      if(openMenu)toggleAlarm(menuState);    
                      buttonTimer = millis();
                      //Serial.("Button3");
                      redraw = true;
  }
  
 //menu button 
  if(exitb.uniquePress()){
                       if(!openMenu){openMenu = true;lcd.clearText();}
                      else home();
                       buttonTimer = millis();
                      redraw = true;
  }
  
  //mode button 
  if(modeb.uniquePress()){
//                      changeMode();
//                      buttonTimer = millis();
//                      redraw = true;
  }

  
  if((millis() - buttonTimer) > (HOME_TIMEOUT * 1000) && (viewEvent != 1 || openMenu))home();     
#endif
}
void home(void){
        viewEvent = 1;
        menuState = 0;
        openMenu = false;
#ifdef LCD        
        lcd.clearText();
#endif        
        //Serial.println("Home");
        redraw = true;
}

void toggleAlarm(int menuState){
  
    if(doors[menuState].alarm){
        doors[menuState].alarm = 0;
        EEPROM.write(menuState, 0);
    }
    else{
        doors[menuState].alarm = 1;
        EEPROM.write(menuState, 1);
    }
    
 
}
void drawMenu(void){
 #ifdef LCD 
     //header
         lcd.TextGoTo(0,2);
         sprintf(line, "    Room    Alarmed    Last Motion");
         printLine(line);
         lcd.TextGoTo(0,3);
         lcd.writeString("----------------------------------------");
       //header
  
  for(int i = 0; i < lcdLines-4; i++){

      lcd.TextGoTo(0,i+4);
     
      if(menuState + i < maxDoors){
         DateTime time (doors[menuState + i].stamp); 
         if(doors[menuState + i].stamp){
           sprintf(line, "%s %+3s     %s    %02d:%02d %02d-%02d-%02d" ,(!i ? "-->":"   "), doors[menuState + i].name, (doors[menuState + i].alarm ? "Enabled":"Off    "), time.hour(), time.minute(), time.day(), time.month(), time.year());
         }else{
            sprintf(line, "%s %+3s     %s" ,(!i ? "-->":"   "), doors[menuState + i].name, (doors[menuState + i].alarm ? "Enabled":"Off    "));
         }
         printLine(line);       
          
      }else emptyLine();
      
   }
#endif
}

void drawEvents(){
#ifdef LCD
//header
/*
    lcd.TextGoTo(0,2);
    sprintf(line, "  Latest       Time     Alerted");
    printLine(line);
    lcd.TextGoTo(0,3);
    lcd.writeString("----------------------------------------");
*/
//header


   for(int i = 0; i < lcdLines-2; i++){
       
        lcd.TextGoTo(0,i+2);
        
        int transEvent = latestEvent - viewEvent - i + 1;
        if(transEvent < 1)transEvent = maxEvents + transEvent;//loop around
        
        if(events[transEvent].stamp == 0){
          emptyLine();
          continue;
        }
      
        DateTime time (events[transEvent].stamp);
        sprintf(line, "    Room %+3s     %02d:%02d:%02d   %02d-%02d-%02d", events[transEvent].name, time.hour(), time.minute(), time.second(),  time.day(), time.month(), time.year());
        printLine(line);      
        
   }
#endif
}

void printLine(char* line){
#ifdef LCD  
  sprintf(line, "%-39s", line);
  lcd.writeString(line);  
#endif
}

void emptyLine(){
#ifdef LCD  
  sprintf(line, "%-39s", "");
  lcd.writeString(line);  
#endif  
}
int eventCount(void){    
  
  if(events[maxEvents].stamp == 0)return latestEvent;
  else return maxEvents;  
  
}

int freeRam ()
{
    extern int __heap_start, *__brkval;
    int v;
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

void logEvent(int id, byte type){
 
#ifdef SD
  DateTime time (events[id].stamp);
  if(type)time = RTC.now();
  
  char logName[13];
  sprintf(logName, "%04d%02d%02d.CSV", time.year(), time.month(), time.day());

  char timeString[20];
  sprintf(timeString, "%04d-%02d-%02d %02d:%02d:%02d", time.year(), time.month(), time.day(), time.hour(), time.minute(), time.second());
    
  boolean newLog = false;  
  
  if (!sd.begin(chipSelect, SPI_HALF_SPEED)){
#ifdef LCD
      lcd.clearText();
      lcd.TextGoTo(0,0);
      lcd.writeString("SD ERROR...");
#endif      
      sd.initErrorHalt();
  }

  if(!sd.exists(logName))newLog = true;
    // open stream for append
    ofstream sdout(logName, ios::out | ios::app);
     
   if(newLog)sdout << "NAME,STATUS,ALARMED,TIME" << endl;
       
   switch(type){
      case 0:
             sdout << events[id].name  << ",";
             sdout << "MOTION" << ",";
             if(events[id].alarmed){sdout << "YES" << ",";}else{sdout << "NO" << ",";}
       break;       
      case 1:
           sdout << "SYSTEM,START,NO,";
       break;       
      case 2:
           sdout << "MODE," << modes[mode] << ",NO,";
       break;         
      case 3:
           sdout << doors[id].name << ",CLEAR,NO,";
       break;
    }      
       

       sdout << timeString;
       sdout << endl;

    // close the stream
    sdout.close();
    
#endif

}

void changeMode(void){
      
      mode++;
      if(mode > 2)mode = 0;
      EEPROM.write(maxDoors, mode);
      logEvent(0,2);
     
}

void sendEvents(boolean force){  // cast recent 5 events
#ifdef CAST

  static int currentRelay = 0;
  static boolean queue = false;
  
  if(force){
    Serial.println("[Cast] Queued");
    currentRelay = 0;
    queue = true;
    return;
  }
  
  if(millis() - castDelay < CAST_DELAY * 1000)return;//delay between individual transmissions
  
  if(!queue)if(millis() - castTimer < (CAST_EVERY * 1000))return;//interval between casts
  
   int t = 0;
   
//empty casts
   for(int c = 0; c < maxCast; c++){
       casts[c].id = 0;
       casts[c].hours = 0;
       casts[c].mins = 0;
   }
   
//get recent events
   for(int i = 0; i < maxEvents; i++){
   
     int transEvent = latestEvent - i;
     if(transEvent < 1)transEvent = maxEvents + transEvent;//loop around
   
     if(events[transEvent].alarmed == 1 /*&& events[transEvent].status == 1*/){//uncleared alarmed and door opened
       
       casts[t].id = events[transEvent].doorId;
       
       DateTime time (events[transEvent].stamp);
       casts[t].hours = time.hour();
       casts[t].mins = time.minute();

        t++;
     }
     if(t >= maxCast)break;
   } 
   
//send events
    Serial.print("[Cast] Relay: ");

          char address[5];
          sprintf(address, "0%o", mobileRelays[currentRelay]);

    Serial.print(address);
     
    RF24NetworkHeader header(mobileRelays[currentRelay], 'B');
       
        bool ok = network.write(header, &casts, sizeof(casts));
        Serial.print(" Result: ");
        Serial.println(ok);

    currentRelay++;
    queue = false;  
    castDelay = millis();
    
    if(currentRelay == mobileRelaysCount){//done all relays reset
        currentRelay = 0;
        castTimer = millis();
    }

#endif  
}

