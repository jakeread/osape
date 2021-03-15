/*
osap/vertex.h

graph vertex 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef VERTEX_H_
#define VERTEX_H_

#include <Arduino.h> 
#include "ts.h"

#define VT_STACKLEN 512
#define VT_STACKSIZE 4
#define VT_MAXCHILDREN 64 
#define VT_STACK_ORIGIN 0 
#define VT_STACK_DESTINATION 1 

// https://stackoverflow.com/questions/1813991/c-structure-with-pointer-to-self

typedef struct vertex_t vertex_t;

struct vertex_t {
    // a loop code, run once per turn 
    void (*loop)(void) = nullptr;
    // a type, a position, a name 
    uint8_t type = 0;
    uint16_t indice = 0;
    String name = "unnamed vertex";
    // stacks; 
    // origin stack[0] destination stack[1]
    // destination stack is for messages delivered to this vertex, 
    uint8_t stack[2][VT_STACKSIZE][VT_STACKLEN];
    uint8_t stackSize = VT_STACKSIZE; // should be variable 
    uint8_t lastStackHandled[2] = { 0, 0 };
    //uint8_t stackHead[2] = { 0, 0 };  // data loads into the head 
    //uint8_t stackTail[2] = { 0, 0 };  // data consumed from the tail 
    uint16_t stackLengths[2][VT_STACKSIZE] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } }; // ugly... and these should be variable 
    unsigned long stackArrivalTimes[2][VT_STACKSIZE];
    // parent & children (other vertices)
    vertex_t* parent = nullptr;
    vertex_t* children[VT_MAXCHILDREN]; // I think this is OK on storage: just pointers 
    uint16_t numChildren = 0;
    // vertex-as-endpoint: token for clear / not to write, fn for writing, and local store 
    uint8_t data[VT_STACKLEN];
    uint16_t dataLen = 0;
    boolean (*onData)(uint8_t* data, uint16_t len) = nullptr;
    // vertex-as-vport-interface 
    boolean (*cts)(uint8_t drop) = nullptr;
    void (*send)(uint8_t* data, uint16_t len, uint8_t rxAddr) = nullptr;
    uint16_t ownRxAddr = 0;
    // to notify for clear-out callbacks / flowcontrol etc 
    void (*onOriginStackClear)(uint8_t slot) = nullptr;
    void (*onDestinationStackClear)(uint8_t slot) = nullptr;
};

// return next clear slot, 
void stackClearSlot(vertex_t* vt, uint8_t od, uint8_t slot);
boolean stackEmptySlot(vertex_t* vt, uint8_t od, uint8_t* slot);
boolean stackNextMsg(vertex_t* vt, uint8_t od, uint8_t* slot);

#endif 