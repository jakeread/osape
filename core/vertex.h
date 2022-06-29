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
#include "stack.h"
// vertex config is build dependent, define in <folder-containing-osape>/osapConfig.h 
#include "./osap_config.h" 

// we have the vertex type, 
// since it contains ptrs to others of its type, we fwd declare the type...
class Vertex;
// ... 
typedef struct stackItem stackItem;
typedef struct VPort VPort;
typedef struct VBus VBus;

// default vt fns 
void vtLoopDefault(Vertex* vt);
void vtOnOriginStackClearDefault(Vertex* vt, uint8_t slot);
void vtOnDestinationStackClearDefault(Vertex* vt, uint8_t slot);

// addressable node in the graph ! 
class Vertex {
  public:
    // just temporary stashes, used all over the place to prep messages... 
    static uint8_t payload[VT_SLOTSIZE];
    static uint8_t datagram[VT_SLOTSIZE];
    // -------------------------------- FN PTRS 
    // these are *genuine function ptrs* not member functions, my dudes 
    void (*loop_cb)(Vertex* vt) = nullptr;
    // to notify for clear-out callbacks / flowcontrol etc 
    void (*onOriginStackClear_cb)(Vertex* vt, uint8_t slot) = nullptr;
    void (*onDestinationStackClear_cb)(Vertex* vt, uint8_t slot) = nullptr;
    // -------------------------------- Methods
    virtual void loop(void);
    virtual void destHandler(stackItem* item, uint16_t ptr);
    void pingRequestHandler(stackItem* item, uint16_t ptr);
    void scopeRequestHandler(stackItem* item, uint16_t ptr);
    virtual void onOriginStackClear(uint8_t slot);
    virtual void onDestinationStackClear(uint8_t slot);
    // -------------------------------- DATA
    // a type, a position, a name 
    uint8_t type = VT_TYPE_CODE;
    uint16_t indice = 0;
    String name; 
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
    Vertex* parent = nullptr;
    Vertex* children[VT_MAXCHILDREN]; // I think this is OK on storage: just pointers 
    uint16_t numChildren = 0;
    // sometimes a vertex is a vport, sometimes it is a vbus, 
    VPort* vport;
    VBus* vbus;
    // -------------------------------- CONSTRUCTORS 
    Vertex( 
      Vertex* _parent, 
      String _name, 
      void (*_loop)(Vertex* vt),
      void (*_onOriginStackClear)(Vertex* vt, uint8_t slot),
      void (*_onDestinationStackClear)(Vertex* vt, uint8_t slot)
    );
    Vertex(Vertex* _parent, String _name) : Vertex(_parent, _name, nullptr, nullptr, nullptr){};
    Vertex(String _name) : Vertex(nullptr, _name, nullptr, nullptr, nullptr){};
};

// ---------------------------------------------- VPort 

class VPort : public Vertex {
  public:
    // -------------------------------- FN *PTRS* ... not methods 
    void (*send_cb)(VPort* vp, uint8_t* data, uint16_t len) = nullptr;
    boolean (*cts_cb)(VPort* vp) = nullptr;
    boolean (*isOpen_cb)(VPort* vp) = nullptr;
    // -------------------------------- OK these bbs are methods, 
    virtual void send(uint8_t* data, uint16_t len);
    virtual boolean cts(void);
    virtual boolean isOpen(void);
    // base constructor, 
    VPort( 
      Vertex* _parent, String _name,
      void (*_loop)(Vertex* vt),
      void (*_send)(VPort* vp, uint8_t* data, uint16_t len),
      boolean (*_cts)(VPort* vp),
      boolean (*_isOpen)(VPort* vp),
      void (*_onOriginStackClear)(Vertex* vt, uint8_t slot),
      void (*_onDestinationStackClear)(Vertex* vt, uint8_t slot)
    );
    // and the delegates,
    // one w/ just origin stack, 
    VPort(
      Vertex* _parent, String _name,
      void (*_loop)(Vertex* vt),
      void (*_send)(VPort* vp, uint8_t* data, uint16_t len),
      boolean (*_cts)(VPort* vp),
      boolean (*_isOpen)(VPort* vp),
      void (*_onOriginStackClear)(Vertex* vt, uint8_t slot)
    ) : VPort (
      _parent, _name, _loop, _send, _cts, _isOpen, _onOriginStackClear, nullptr
    ){};
    // one w/ no stack callbacks, 
    VPort(
      Vertex* _parent, String _name,
      void (*_loop)(Vertex* vt),
      void (*_send)(VPort* vp, uint8_t* data, uint16_t len),
      boolean (*_cts)(VPort* vp),
      boolean (*_isOpen)(VPort* vp)
    ) : VPort (
      _parent, _name, _loop, _send, _cts, _isOpen, nullptr, nullptr
    ){};
    // w/ no CTS
    VPort(
      Vertex* _parent, String _name,
      void (*_loop)(Vertex* vt),
      void (*_send)(VPort* vp, uint8_t* data, uint16_t len)
    ) : VPort (
      _parent, _name, _loop, _send, nullptr, nullptr, nullptr
    ){};
    // w/ just parent & name... 
    VPort(
      Vertex* _parent, String _name
    ) : VPort (
      _parent, _name, nullptr, nullptr, nullptr, nullptr, nullptr
    ){};
};

// ---------------------------------------------- VBus 

struct VBus : public Vertex{
  public:
    // -------------------------------- FN *PTRS* ... not methods 
    void (*send_cb)(VBus* vb, uint8_t* data, uint16_t len, uint8_t rxAddr) = nullptr;
    void (*broadcast_cb)(VBus* vb, uint8_t* data, uint16_t len, uint8_t broadcastChannel) = nullptr;
    boolean (*cts_cb)(VBus* vb, uint8_t rxAddr) = nullptr;
    boolean (*ctb_cb)(VBus* vb, uint8_t broadcastChannel) = nullptr;
    // -------------------------------- Methods: these are purely virtual... 
    virtual void send(uint8_t* data, uint16_t len, uint8_t rxAddr) = 0;
    virtual void broadcast(uint8_t* data, uint16_t len, uint8_t broadcastChannel) = 0;
    // clear to send, clear to broadcast, 
    virtual boolean cts(uint8_t rxAddr) = 0;
    virtual boolean ctb(uint8_t broadcastChannel) = 0;
    // link state, 
    virtual boolean isOpen(uint8_t rxAddr) = 0;
    // has an rx addr, 
    uint16_t ownRxAddr = 0;
    // has a width-of-addr-space, 
    uint16_t addrSpaceSize = 0;
    // base constructor, children inherit... 
    VBus( Vertex* _parent, String _name );
};


#endif 