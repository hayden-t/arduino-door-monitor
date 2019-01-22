#include <SPI.h>
#include <RF24.h>
#include "printf.h"
#include <LCD5110_Basic.h>
#include <Button.h>
#include "doors.h"
#include "graphics.h"
#include <RunningAverage.h>
#include <EEPROM.h>

int channel = 84;

LCD5110 lcd(4,7,6,8,5);
extern uint8_t SmallFont[];

Button power = Button(2, BUTTON_PULLUP_INTERNAL);
//Button clear = Button(A2, BUTTON_PULLUP_INTERNAL);
//Button menu = Button(A3, BUTTON_PULLUP_INTERNAL);

const int MENU_TIMEOUT = 10 * 1000;
const int LCD_TIMEOUT = 30 * 1000;

const int LOW_VOLTAGE = 0;//%
const int MIN_VOLTAGE = 30;//V

const int RX_TIMEOUT = 2;//delay since last recieve to decrement signal meter
//const int RX_ALERT = 30;//delay since last recieve to alert signal lost
//maximum total delay between individual relay = CAST_DELAY + CAST_EVERY

const unsigned long CHARGE_TIME = 900;//minutes

const int ALARM_GAP = 7;//seconds

const int CHARGE_DETECT = A1;
const int CHARGE_CONTROL = A2;

const int POWER_CONTROL = A3;

const int BUZZER_PIN = A4;

const int LCD_BACKLIGHT = 3;

RunningAverage VOLTAGE(50);

char line[15];

int lcdLines = 6;

typedef struct {
  byte id;
  byte hours;
  byte mins;
} cast;

const int maxCast = 5;

cast casts[maxCast];//used to broadcast
cast oldCast;//used to to compare for new

//cast latestEvent;

const uint64_t pipes[6] = { 0xf04b962d3cLL, 0xf04b962d5aLL, 0xf04b962d69LL, 0xf04b962d96LL, 0xf04b962da5LL, 0xf04b962dc3LL };

const uint64_t readPipe = pipes[0];//mobile pipe number - common to mobile units
//const uint64_t sendPipe = pipes[5];//mobile pipe number - unique to mobile units

RF24 radio(9,10);

unsigned long lastMenu;
int menuState = -1;

unsigned long lastClear;
int charging;
unsigned long chargeTimer;
unsigned long timeLeft;
unsigned long lowVoltTimer;

boolean alarm = false;
boolean connected = false;

void setup(void)
{
  
  //EEPROM.write(0, 2);//save unit number

  Serial.begin(9600);
  printf_begin();
  
  lcd.InitLCD();
  lcd.setFont(SmallFont);
  
  pinMode(LCD_BACKLIGHT, OUTPUT);//lcd backlight
  digitalWrite(LCD_BACKLIGHT, LOW);//lcd backlight  
 
  pinMode(CHARGE_DETECT, INPUT_PULLUP);//charge detect
  pinMode(CHARGE_CONTROL, OUTPUT);//charge control
  digitalWrite(CHARGE_CONTROL, LOW);//charge off  
  
  pinMode(POWER_CONTROL, OUTPUT);//low volt control
  digitalWrite(POWER_CONTROL, HIGH);//low volt control
  
  
  checkCharge();
  power.holdHandler(powerCallback, 3000); // must be held for at least 1000 ms to trigger

  printf("\n\rmobile_rx\n\n\r");
  radio.begin();
  
  radio.setChannel(channel);
 // radio.setPALevel(RF24_PA_LOW);//-12dbm  
  radio.setDataRate(RF24_250KBPS);  
  
  radio.openReadingPipe(1, readPipe);
  radio.setAutoAck(false) ;
  
  radio.powerUp() ;  
  radio.printDetails(); 
  radio.startListening();
  
  sprintf(line, "MOBILE UNIT #%d", EEPROM.read(0));
  lcd.print(line, CENTER, 8); 
  int volts = readVcc();
  sprintf(line, "BATT: %d.%dv", int(volts/10), volts-(int(volts/10)*10));
  lcd.print(line, CENTER, 24); 
 // sprintf(line, "MEM: %d",  freeRam());
 // lcd.print(line, CENTER, 40);
  
   
  chime();
  delay(3000);
  lcd.clrScr();   

 
}
int rx = 0;
unsigned long lastRx;
void loop(void)
{
    if(lastMenu > millis())lastMenu = millis();//check for overflow
    if(lastClear > millis())lastClear = millis();//check for overflow
    if(chargeTimer > millis())chargeTimer = millis();//check for overflow
    if(lowVoltTimer > millis())lowVoltTimer = millis();//check for overflow
    
    if(millis() - lastRx > RX_TIMEOUT * 1000){
        if(rx > 0)rx--;
        else if(connected)signalLost();
        lastRx = millis();
    }
  
    // if there is data ready
    if ( radio.available() )
    {
        
       radio.read(&casts, sizeof(casts));
       
       if(rx == 0){
         Serial.println();
         Serial.print("[Casts] ");         
       }
       
       if(rx < 14)rx++;
       else Serial.println();
       
       lastRx = millis();
       //lcd.print("     ", 21, 0);//remove clear
       connected = true;
        
        Serial.print(rx);
        Serial.print(", ");
       /* int show = 0;
        Serial.print(casts[show].id);
        Serial.print(" ");
        Serial.print(casts[show].status);
        Serial.print(" ");
        Serial.print(casts[show].time);
        Serial.println();       */   
        

        
       /* 
        if(casts[0].id != latestEvent.id || casts[0].hours != latestEvent.hours || casts[0].mins != latestEvent.mins){
          //new event
          if(!charging){
              alarm = true;
              lastMenu = millis();
              latestEvent.id = casts[0].id;
              latestEvent.hours = casts[0].hours;
              latestEvent.mins = casts[0].mins;
          }
        }
        */
    }
    
    checkButtons();    
    checkCharge();
    checkAlarm();
    drawScreen();       
}

void checkButtons(){
 /* if(clear.uniquePress()){
     //sendClear();
     alarm = false;
     lastMenu = millis();
     tone(BUZZER_PIN,4000,50);
  }else if(menu.uniquePress()){
     //shiftMenu();
     alarm = false;
     lastMenu = millis();
     tone(BUZZER_PIN,4000,50);
  }else*/ if(power.uniquePress()){
     lastMenu = millis();
     alarm = false;
     oldCast.id = casts[0].id; oldCast.hours = casts[0].hours; oldCast.mins =  casts[0].mins;
     tone(BUZZER_PIN,4000,50);
  }
  
  power.process();
  
  if(millis() - lastMenu > MENU_TIMEOUT)menuState = -1;
  
  if(millis() - lastMenu > LCD_TIMEOUT){
      digitalWrite(LCD_BACKLIGHT, HIGH);//lcd backlight off
  }else{
      digitalWrite(LCD_BACKLIGHT, LOW);//lcd backlight on
  }
}

void drawScreen(){
  
     //  alarm = false;    
   //header
       
       lcd.drawBitmap(0, 0, tower, 7, 8);       
           
       switch(int(rx/2)){
         case 0:
           lcd.drawBitmap(7, 0, loading, 14, 8);
         break;
         case 1:
           lcd.drawBitmap(7, 0, signal1, 14, 8);
           break;
         case 2:
           lcd.drawBitmap(7, 0, signal2, 14, 8);
           break;       
         case 3:
           lcd.drawBitmap(7, 0, signal3, 14, 8);
           break;        
         case 4:
           lcd.drawBitmap(7, 0, signal4, 14, 8);
           break;
         case 5:
           lcd.drawBitmap(7, 0, signal5, 14, 8);
           break;           
         case 6:
           lcd.drawBitmap(7, 0, signal6, 14, 8);
           break;
         case 7:
           lcd.drawBitmap(7, 0, signal7, 14, 8);
           break;
       };    
       
     int volts = readVcc();
     if(charging)volts -= 2;
     constrain(volts, 30, 42);
     int percent = map(volts, 30, 42, 0, 10);
     percent *= 10;
     if(percent == 100)percent = 99;
     sprintf(line, "%2d%%", percent);
     lcd.print(line, RIGHT, 0);       
      
     if(percent <= LOW_VOLTAGE && !charging && millis() - lowVoltTimer > 10000){
        tone(BUZZER_PIN, 4000, 500);
        lowVoltTimer = millis();
      }
      
      
           if(percent <= LOW_VOLTAGE && (millis() % 1000 < 500)){               
               lcd.clrRow(0, 52, 66);
           }
           else{
               if(percent>=90)lcd.drawBitmap(52, 0, battery9, 14, 8);
               else if(percent>=80)lcd.drawBitmap(52, 0, battery8, 14, 8);
               else if(percent>=70)lcd.drawBitmap(52, 0, battery7, 14, 8);
               else if(percent>=60)lcd.drawBitmap(52, 0, battery6, 14, 8);
               else if(percent>=50)lcd.drawBitmap(52, 0, battery5, 14, 8);
               else if(percent>=40)lcd.drawBitmap(52, 0, battery4, 14, 8);
               else if(percent>=30)lcd.drawBitmap(52, 0, battery3, 14, 8);
               else if(percent>=20)lcd.drawBitmap(52, 0, battery2, 14, 8);
               else if(percent>=10)lcd.drawBitmap(52, 0, battery1, 14, 8);
               else lcd.drawBitmap(52, 0, battery0, 14, 8);
           }

       if(alarm)lcd.print(" NEW ", 21, 0);
       else lcd.print("     ", 21, 0);
       
  //header
  boolean match = false;
  //events
    if(connected && !charging){
       //lcd event lines
          for(int lcdline = 1; lcdline < lcdLines; lcdline++){
  
            int castId = lcdline - 1;//lcd offset
            
            if(!casts[castId].hours && !casts[castId].mins){lcd.clrRow(lcdline);continue;}//empty line
            

            sprintf(line, "%s %s %02d:%02d","ROOM", doors[casts[castId].id].name, casts[castId].hours, casts[castId].mins );//format
   
            if(casts[castId].id != oldCast.id || casts[castId].hours != oldCast.hours || casts[castId].mins != oldCast.mins){//new?
               if(!match){
                    alarm = true;
                    lastMenu = millis();
               }             
            }else{
                    match = true;  
            }
                    if(!match)lcd.invertText(true);                     
                        lcd.print(line, LEFT, 8 * lcdline);//print content                    
                    if(!match)lcd.invertText(false);

            
          }
             //events  
    }else{
      
          for(int lcdline = 1; lcdline < lcdLines; lcdline++){
            
              switch(lcdline){
                
                case 2:
                       if(!connected)lcd.print("  NO  SIGNAL  ", CENTER, lcdline*8);
                       else lcd.clrRow(lcdline);
                  break;             
                case 4:
                      if(charging){
                        int volts = readVcc();
                        sprintf(line, "CHARGING %d.%dv", int(volts/10), volts-(int(volts/10)*10));
                        lcd.print(line, CENTER, lcdline*8);
                      }
                      else lcd.clrRow(lcdline);                
                  break;
                  
                case 5:                
                      if(charging){
                         int hours = timeLeft / 3600;
                         int mins = (timeLeft - ((unsigned long)hours*3600)) / 60;
                         int sec = timeLeft - ((unsigned long)hours*3600) - (mins*60);
                         Serial.print(timeLeft);
                         Serial.print(" ");
                         Serial.print(hours);
                         Serial.print(" ");
                         Serial.print(mins);
                         Serial.println();
                         sprintf(line, "%02d:%02d:%02d", hours, mins, sec );//format
                         lcd.print(line, CENTER, lcdline*8);
                      }else lcd.clrRow(lcdline); 
                
                  break;  
                  
                default:
                      lcd.clrRow(lcdline);
                break;                  
              }            
          }      
    } 

 //body
}

int readVcc() {

  static long result;
  
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA, ADSC));
  result = ADCL;
  result |= ADCH << 8;
  result = 1126400L / result; // Back-calculate AVcc in mV 
  
   result /= 100;
  
  VOLTAGE.addValue(int(result));
  result = VOLTAGE.getAverage();
  
  if(result <= MIN_VOLTAGE)powerOff();
  
  return result;
}
int freeRam ()
{
    extern int __heap_start, *__brkval;
    int v;
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

void checkCharge(){
    
   static int plugged = 0;  
      
   if(digitalRead(CHARGE_DETECT) != plugged){//plugged in or out
     plugged = digitalRead(CHARGE_DETECT);
     if(plugged){//plugged in
         chargeTimer = millis();
         digitalWrite(CHARGE_CONTROL, HIGH);//charger on
         charging = 1;
         Serial.println("Charge Started");
         tone(BUZZER_PIN, 4000,100);
         lastMenu = millis();
         lcd.clrScr();
         alarm = false;
     }else{//unplugged
       digitalWrite(CHARGE_CONTROL, LOW);//charger off
       charging = 0;
       Serial.println("Charge Cancelled");
       tone(BUZZER_PIN, 3800,100);
       lastMenu = millis();
       lcd.clrScr();
     }     
   }
  
   if(charging && millis() - chargeTimer > CHARGE_TIME * 60000){//timer up
       digitalWrite(CHARGE_CONTROL, LOW);//charger off
       charging = 0;
       Serial.println("Charge Finished");
       powerOff();
       lcd.clrScr();
   }
   
   if(charging){
      timeLeft =  ((CHARGE_TIME * 60000) - (millis() - chargeTimer))/1000;//countdown
      //timeLeft  = (millis() - chargeTimer)/1000;//countup
   }
  
}

void chime(){
  tone(BUZZER_PIN, 4000,75);
  delay(150);
  
  tone(BUZZER_PIN, 3900,75);
  delay(150);
  
  tone(BUZZER_PIN, 4100,75);
  delay(150);
  
  tone(BUZZER_PIN, 3800,75);
  delay(150);
}


void checkAlarm(){
  
  static unsigned long alarmTimer = millis();
  static int note = 0;
  int noteGap = 350;
  static boolean pause = false;
  
  //alarm = true;
  
  if(!alarm){
    pause = false;
    note = 0;
    return;
  }  
  
  if(pause){
      if(millis() - alarmTimer > (ALARM_GAP*1000))pause = false;
      else return;
  }

  if(millis() - alarmTimer > noteGap*note){//gap between notes
      switch(note){
           case 0:
               tone(BUZZER_PIN, 4100, 300);
               note++;
               alarmTimer = millis();
             break;     
           case 1:     
               tone(BUZZER_PIN, 3900, 100);
               note++;
             break;
           case 2:     
               //tone(BUZZER_PIN, 4100, 75);
               note++;
             break;
           case 3:     
               //tone(BUZZER_PIN, 4100, 75);
               note++;     
             break;     
           case 4:
               note = 0;  
               pause = true;
             break;    
           default:
             break;
      }
     
  }

}

void powerCallback(Button& b){
  powerOff();
}

void powerOff(){
  if(charging)return;
  tone(BUZZER_PIN, 4000,1000);
  delay(500);
  digitalWrite(POWER_CONTROL, LOW);//turn off
  digitalWrite(LCD_BACKLIGHT, HIGH);//turn lcd off
  lcd.clrScr();
  while(1);//infinite loop till power button released
}

void signalLost(){
  
    connected = false;
    alarm = false;
    
    casts[0].hours, casts[0].mins = 0;
    casts[1].hours, casts[1].mins = 0;
    casts[2].hours, casts[2].mins = 0;
    casts[3].hours, casts[3].mins = 0;
    casts[4].hours, casts[4].mins = 0;
    
    tone(BUZZER_PIN, 4000,250);
    delay(500);
    tone(BUZZER_PIN, 4000,250);
    delay(500);
    tone(BUZZER_PIN, 4000,250);
    delay(500);
    
    lcd.clrScr();
}
