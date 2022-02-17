/*
osap/vertex.h

graph vertex 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2020

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef VERTEX_H_
#define VERTEX_H_

#include <Arduino.h> 
#include "ts.h"
// vertex config is build dependent, define in <folder-containing-osape>/vertices/vertexConfig.h 
#include "../../vertices/vertexConfig.h" 

#define VT_STACK_ORIGIN 0 
#define VT_STACK_DESTINATION 1 

// core routing layer chunk-of-stuff, 
// https://stackoverflow.com/questions/1813991/c-structure-with-pointer-to-self
struct stackItem {
  uint8_t data[VT_SLOTSIZE];          // data bytes
  uint16_t len = 0;                   // data bytes count 
  unsigned long arrivalTime = 0;      // microseconds-since-system-alive, time at last hop 
  uint8_t indice;                     // actual physical position in the stack 
  stackItem* next = nullptr;          // linked ringbuffer next 
  stackItem* previous = nullptr;      // linked ringbuffer previous 
};

// we have the vertex type, 
// since it contains ptrs to others of its type, we fwd declare the type...
typedef struct vertex_t vertex_t;
typedef struct vport_t vport_t;
typedef struct vbus_t vbus_t;

// default vt fns 
void vtLoopDefault(vertex_t* vt);
void vtOnOriginStackClearDefault(vertex_t* vt, uint8_t slot);
void vtOnDestinationStackClearDefault(vertex_t* vt, uint8_t slot);

struct vertex_t {
  // a type, a position, a name 
  uint8_t type = VT_TYPE_CODE;
  uint16_t indice = 0;
  String name; 
  // a loop code, run once per turn. depends on the vertex
  void (*loop)(vertex_t* vt) = &vtLoopDefault;
  // a time tag, for when we were last scoped (need for graph traversals, final implementation tbd)
  uint32_t scopeTimeTag = 0;
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
  // sometimes a vertex is a vport, sometimes it is a vbus, 
  vport_t* vport;
  vbus_t* vbus;
  // to notify for clear-out callbacks / flowcontrol etc 
  void (*onOriginStackClear)(vertex_t* vt, uint8_t slot) = &vtOnOriginStackClearDefault;
  void (*onDestinationStackClear)(vertex_t* vt, uint8_t slot) = &vtOnDestinationStackClearDefault;
  // base constructor, 
  vertex_t(vertex_t* _parent, String _name);
  vertex_t(String _name) : vertex_t(nullptr, _name){};
  vertex_t() : vertex_t(nullptr, "vt_unknown"){};
};

// ---------------------------------------------- VPort 

void vpSendDefault(vport_t* vp, uint8_t* data, uint16_t len);
boolean vpCtsDefault(vport_t* vp);

struct vport_t {
  // a vport should contain ahn vertex, these are crosslinked 
  vertex_t vt;
  // to tx, and to check about tx'ing 
  void (*send)(vport_t* vp, uint8_t* data, uint16_t len) = &vpSendDefault;
  boolean (*cts)(vport_t* vp) = &vpCtsDefault;
  // base constructor, 
  vport_t( 
    vertex_t* parent, String _name,
    void (*_loop)(vertex_t* vt),
    void (*_send)(vport_t* vp, uint8_t* data, uint16_t len),
    boolean (*_cts)(vport_t* vp),
    void (*_onOriginStackClear)(vertex_t* vt, uint8_t slot),
    void (*_onDestinationStackClear)(vertex_t* vt, uint8_t slot)
  );
  // and the delegates,
  // one w/ just origin stack, 
  vport_t(
    vertex_t* parent, String _name,
    void (*_loop)(vertex_t* vt),
    void (*_send)(vport_t* vp, uint8_t* data, uint16_t len),
    boolean (*_cts)(vport_t* vp),
    void (*_onOriginStackClear)(vertex_t* vt, uint8_t slot)
  ) : vport_t (
    parent, _name, _loop, _send, _cts, _onOriginStackClear, nullptr
  ){};
  // one w/ no stack callbacks, 
  vport_t(
    vertex_t* parent, String _name,
    void (*_loop)(vertex_t* vt),
    void (*_send)(vport_t* vp, uint8_t* data, uint16_t len),
    boolean (*_cts)(vport_t* vp)
  ) : vport_t (
    parent, _name, _loop, _send, _cts, nullptr, nullptr
  ){};
};

// ---------------------------------------------- VBus 

void vbSendDefault(vbus_t* vb, uint8_t* data, uint16_t len, uint8_t rxAddr);
boolean vbCtsDefault(vbus_t* vb, uint8_t rxAddr);

struct vbus_t {
  // crosslinked etc 
  vertex_t vt;
  // tx, and check abt it 
  void (*send)(vbus_t* vb, uint8_t* data, uint16_t len, uint8_t rxAddr) = &vbSendDefault;
  boolean (*cts)(vbus_t* vb, uint8_t rxAddr) = &vbCtsDefault;
  // has an rx addr, 
  uint16_t ownRxAddr = 0;
  // base constructor, 
  vbus_t( 
    vertex_t* parent, String _name,
    void (*_loop)(vertex_t* vt),
    void (*_send)(vbus_t* vb, uint8_t* data, uint16_t len, uint8_t rxAddr),
    boolean (*_cts)(vbus_t* vb, uint8_t rxAddr),
    void (*_onOriginStackClear)(vertex_t* vt, uint8_t slot),
    void (*_onDestinationStackClear)(vertex_t* vt, uint8_t slot)
  );
  // and the delegates,
  // one w/ just origin stack, 
  vbus_t(
    vertex_t* parent, String _name,
    void (*_loop)(vertex_t* vt),
    void (*_send)(vbus_t* vb, uint8_t* data, uint16_t len, uint8_t rxAddr),
    boolean (*_cts)(vbus_t* vb, uint8_t rxAddr),
    void (*_onOriginStackClear)(vertex_t* vt, uint8_t slot)
  ) : vbus_t (
    parent, _name, _loop, _send, _cts, _onOriginStackClear, nullptr
  ){};
  // one w/ no stack callbacks, 
  vbus_t(
    vertex_t* parent, String _name,
    void (*_loop)(vertex_t* vt),
    void (*_send)(vbus_t* vb, uint8_t* data, uint16_t len, uint8_t rxAddr),
    boolean (*_cts)(vbus_t* vb, uint8_t rxAddr)
  ) : vbus_t (
    parent, _name, _loop, _send, _cts, nullptr, nullptr
  ){};
};

// ---------------------------------------------- Stack Tools 

// stack setup / reset 
void stackReset(vertex_t* vt);

// stack origin side 
boolean stackEmptySlot(vertex_t* vt, uint8_t od);
void stackLoadSlot(vertex_t* vt, uint8_t od, uint8_t* data, uint16_t len);

// stack exit side 
uint8_t stackGetItems(vertex_t* vt, uint8_t od, stackItem** items, uint8_t maxItems);
void stackClearSlot(vertex_t* vt, uint8_t od, stackItem* item);

#endif 