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
#include "../../vertices/vertexConfig.h" // vertex config is build dependent, define in <folder-containing-osape>/vertices/vertexConfig.h 
#include "endpoint.h"

#define VT_STACK_ORIGIN 0 
#define VT_STACK_DESTINATION 1 

// https://stackoverflow.com/questions/1813991/c-structure-with-pointer-to-self

// linked ringbuffer item 
struct stackItem {
    uint8_t data[VT_SLOTSIZE];
    uint16_t len = 0;
    unsigned long arrivalTime = 0;
    uint8_t indice;     // actual physical position in the stack 
    stackItem* next = nullptr;
    stackItem* previous = nullptr;
};

// we have the vertex type, 
typedef struct vertex_t vertex_t;
// which are typed by existence of some other object, 
typedef struct endpoint_t endpoint_t;
typedef struct vport_t vport_t;
typedef struct vbus_t vbus_t;
typedef struct root_t root_t;

struct vertex_t {
    // a loop code, run once per turn
    void (*loop)() = nullptr;

    // a type, a position, a name 
    uint8_t type = 0;
    uint16_t indice = 0;
    // a time tag, for when we git scoped (need for graph traversals)
    uint32_t scopeTimeTag = 0;
    // addnl' properties, can possess:
    endpoint_t* ep = nullptr; 
    vport_t* vp = nullptr;
    vbus_t* vb = nullptr;
    root_t* rt = nullptr; 
    // stacks; 
    // origin stack[0] destination stack[1]
    // destination stack is for messages delivered to this vertex, 
    stackItem stack[2][VT_STACKSIZE];
    uint8_t stackSize = VT_STACKSIZE; // should be variable 
    //uint8_t lastStackHandled[2] = { 0, 0 };
    stackItem* queueStart[2] = { nullptr, nullptr };    // data is read from the tail  
    stackItem* firstFree[2] = { nullptr, nullptr };     // data is loaded into the head 
    // parent & children (other vertices)
    vertex_t* parent = nullptr;
    vertex_t* children[VT_MAXCHILDREN]; // I think this is OK on storage: just pointers 
    uint16_t numChildren = 0;
    // vertex-as-vport-interface 
    boolean (*cts)(uint8_t drop) = nullptr;
    void (*send)(uint8_t* data, uint16_t len, uint8_t rxAddr) = nullptr;
    uint16_t ownRxAddr = 0;
    // to notify for clear-out callbacks / flowcontrol etc 
    void (*onOriginStackClear)(uint8_t slot) = nullptr;
    void (*onDestinationStackClear)(uint8_t slot) = nullptr;
};

void stackReset(vertex_t* vt);

// ORIGIN SIDE 
boolean stackEmptySlot(vertex_t* vt, uint8_t od);
void stackLoadSlot(vertex_t* vt, uint8_t od, uint8_t* data, uint16_t len);

// EXIT SIDE 
uint8_t stackGetItems(vertex_t* vt, uint8_t od, stackItem** items, uint8_t maxItems);
void stackClearSlot(vertex_t* vt, uint8_t od, stackItem* item);

#endif 