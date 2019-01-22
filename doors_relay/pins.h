typedef struct{
  byte id;
  byte pin;
  byte state;
  unsigned long last;
  boolean queue;
} pin;

#ifdef RELAY01
// Address of our node
const uint16_t this_node = 01;
const int numPins = 8;
pin pins[numPins] = {
  {2, 27, 1},//door id //door pin //door state
  {3, 29, 1},
  {4, 31, 1},
  {5, 33, 1},
  {6, 35, 1},
  {7, 37, 1},
  {35, 39, 1},
  {36, 41, 1}
};
#endif

#ifdef RELAY011
// Address of our node
const uint16_t this_node = 011;
const int numPins = 11;
pin pins[numPins] = {
  {0, 27, 1},//door id //door pin //door state
  {1, 29, 1},
  {19, 31, 1},
  {20, 33, 1},
  {21, 35, 1},
  {22, 37, 1},
  {23, 39, 1},
  {24, 41, 1},
  {25, 43, 1},
  {37, 45, 1},
  {38, 47, 1}
};
#endif

#ifdef RELAY021
// Address of our node
const uint16_t this_node = 021;
const int numPins = 5;
pin pins[numPins] = {
  {8, 27, 1},//door id //door pin //door state
  {9, 29, 1},
  {10, 31, 1},
  {11, 33, 1},
  {12, 35, 1}
};
#endif

#ifdef RELAY0111
// Address of our node
const uint16_t this_node = 0111;
const int numPins = 6;
pin pins[numPins] = {
  {13, 27, 1},//door id //door pin //door state
  {14, 29, 1},
  {15, 31, 1},
  {16, 33, 1},
  {17, 35, 1},
  {18, 37, 1}
};
#endif

#ifdef RELAY01111
// Address of our node
const uint16_t this_node = 01111;
const int numPins = 9;
pin pins[numPins] = {
  {26, 27, 1},//door id //door pin //door state
  {27, 29, 1},
  {28, 31, 1},
  {29, 33, 1},
  {30, 35, 1},
  {31, 37, 1},
  {32, 39, 1},
  {33, 41, 1},
  {34, 43, 1}
};
#endif
