/*
 * Copyright (c) 2022, Daniel Tabor
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "indicators.h"
#include "modbusvar.h"

#define INDCOUNT 4

typedef struct {
  uint8_t pin;
  uint8_t point_type;
  uint16_t point_addr;
  uint16_t cutoff;
} ind_t;

static ind_t indicators[INDCOUNT];
static uint8_t ind_idx;
static uint8_t digitalValue;
static uint16_t analogValue;


void indicatorsReset() {
  pinMode(PA7,OUTPUT);
  digitalWrite(PA7,LOW);
  indicators[0].pin = PA7;
  indicators[0].point_type = IND_NONE;
  pinMode(PA4,OUTPUT);
  digitalWrite(PA4,LOW);
  indicators[1].pin = PA4;
  indicators[1].point_type = IND_NONE;  
  pinMode(PA1,OUTPUT);
  digitalWrite(PA1,LOW);
  indicators[2].pin = PA1;
  indicators[2].point_type = IND_NONE;
  pinMode(PA0,OUTPUT);
  digitalWrite(PA0,LOW);
  indicators[3].pin = PA0;
  indicators[3].point_type = IND_NONE;
}

int indicatorsConfig(uint8_t idx, uint8_t point_type, uint16_t point_addr, uint16_t cutoff) {
  if( idx > INDCOUNT ) { 
    return 0; 
  }
  if( point_type == IND_DO ) {
    if( !getDO(point_addr,&digitalValue) ) {
      return 0;
    }
  }
  else if( point_type == IND_DI ) {
    if( !getDI(point_addr,&digitalValue) ) {
      return 0;
    }
  }
  else if( point_type == IND_AI ) {
    if( !getAI(point_addr,&analogValue) ) {
      return 0;
    }
  }
  else if( point_type == IND_AO ) {
    if( !getAO(point_addr,&analogValue) ) {
      return 0;
    }
  }
  else {
    return 0;
  }
  indicators[idx].point_type = point_type;
  indicators[idx].point_addr = point_addr;
  indicators[idx].cutoff = cutoff;
  return 1;
}

void indicatorsUpdate() {
  for( ind_idx=0; ind_idx<INDCOUNT; ind_idx++ ) {
    if( indicators[ind_idx].point_type == IND_DO ) {
      digitalValue = 0;
      getDO(indicators[ind_idx].point_addr,&digitalValue);
      digitalWrite(indicators[ind_idx].pin,digitalValue);
    }
    else if( indicators[ind_idx].point_type == IND_DI )  {
      digitalValue = 0;
      getDI(indicators[ind_idx].point_addr,&digitalValue);
      digitalWrite(indicators[ind_idx].pin,digitalValue);
    }
    else if( indicators[ind_idx].point_type == IND_AI ) {
      analogValue = 0;
      getAI(indicators[ind_idx].point_addr,&analogValue);
      if( analogValue >= indicators[ind_idx].cutoff ) {
        digitalWrite(indicators[ind_idx].pin,HIGH);
      }
      else {
        digitalWrite(indicators[ind_idx].pin,LOW);
      }
    }
    else if( indicators[ind_idx].point_type == IND_AO ) {
      analogValue = 0;
      getAO(indicators[ind_idx].point_addr,&analogValue);
      if( analogValue >= indicators[ind_idx].cutoff ) {
        digitalWrite(indicators[ind_idx].pin,HIGH);
      }
      else {
        digitalWrite(indicators[ind_idx].pin,LOW);
      }
    }
  }
}

void indicatorsPrint() {
  for( ind_idx=0; ind_idx<INDCOUNT; ind_idx++ ) {
    if( indicators[ind_idx].point_type == IND_NONE ) {
      continue;
    }
    Serial.print("led ");
    Serial.print(ind_idx);
    if( indicators[ind_idx].point_type == IND_DO ) {
      Serial.print(" do ");
    }
    else if( indicators[ind_idx].point_type == IND_DI ) {
      Serial.print(" di ");
    }
    else if( indicators[ind_idx].point_type == IND_AI ) {
      Serial.print(" ai ");
    }
    Serial.print(indicators[ind_idx].point_addr);
    if( indicators[ind_idx].point_type == IND_AI ) {
      Serial.print(" 0x");
      Serial.println(indicators[ind_idx].cutoff,HEX);
    }
    else {
      Serial.println("");
    }
  }
}
