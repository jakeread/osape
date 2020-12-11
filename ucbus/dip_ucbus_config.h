// DIP switch HAL macros 
// pardon the mis-labeling: on board, and in the schem, these are 1-8, 
// here they will be 0-7 

// note: these are 'on' hi by default, from the factory. 
// to set low, need to turn the internal pulldown on 

#include <Arduino.h>

#define D0_PIN 5
#define D1_PIN 4
#define D2_PIN 3
#define D3_PIN 2
#define D4_PIN 1 
#define D5_PIN 0
#define D6_PIN 31 
#define D7_PIN 30
#define DIP_PORT PORT->Group[1]
#define D_BM(val) ((uint32_t)(1 << val))

void dip_init(void);
uint8_t dip_read_lower_five(void);
boolean dip_read_pin_0(void); // bus-head (hi) or bus-drop (lo)
boolean dip_read_pin_1(void); // if bus-drop, te-enable (hi) or no (lo)