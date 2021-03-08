/*
osap/vertex.cpp

graph vertex 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "vertex.h"

// true if empty space in vertex stack
// *space = first empty 
boolean vertexSpace(vertex_t* vt, uint8_t* space){
  for(uint8_t s = 0; s < vt->stackSize; s ++){
    if(vt->stackLen[s] == 0){
      *space = s;
      return true;
    }
  }
  return false;
}