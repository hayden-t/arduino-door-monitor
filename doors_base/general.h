
typedef struct {
  byte doorId;
  char name[4];
  byte alarmed;
  unsigned long stamp;
} event;

typedef struct {
  byte id;
  byte hours;
  byte mins;
} cast;



const int maxEvents = 200;
event events[maxEvents + 1];//zero indexed, 0 not used for simplicity
event emptyEvent;

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

char* modes[] = {
       "NONE  ",
       "ALL   ",
       "CUSTOM"
 };

const byte ledPin = A7;
const byte BUZZER_PIN = 2;


Button exitb = Button(A0);
Button enter = Button(A1);
Button down = Button(A2);
Button up = Button(A3);
Button modeb = Button(A4);
Button dstb = Button(3, BUTTON_PULLUP_INTERNAL);


