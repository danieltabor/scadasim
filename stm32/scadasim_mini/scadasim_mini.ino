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
#include "expr.h"
#include "modbus.h"
#include "modbusvar.h"
#include "cli.h"
#include "indicators.h"

#define TICKINTERVAL    250

static char* scadasim_line;
static int scadasim_error;
static unsigned long last_tick;
static uint8_t last_loop;

void setup() {
  pinMode(PB14,INPUT_PULLUP);
  pinMode(PB12,OUTPUT);
  pinMode(PB13,OUTPUT);
  randomSeed(analogRead(0));
  
  Serial.begin(9600);
  Serial.println("");
  
  var_init();
  indicatorsReset();
    
  last_loop = 2;
}



void loop() {
  //Check to see if we need to tick the simulation
  if( millis() - last_tick >= TICKINTERVAL ) {
    last_tick = last_tick + TICKINTERVAL;
    expr_tick();
    indicatorsUpdate();
  }
  
  if( digitalRead(PB14) == 0 ) {
    if( last_loop != 0 ) {
      //Change LED color
      digitalWrite(PB12,LOW);
      digitalWrite(PB13,HIGH);
      //Reset for modbus processing
      beginModbus();
      last_loop = 0;
    }
    processModbus();
  }
  else {
    if( last_loop != 1 ) {
      //Change LED color
      digitalWrite(PB13,LOW);
      digitalWrite(PB12,HIGH);
      //Reset for command processing
      cli_init();
      last_loop = 1;
    }
    //Process Console port input
    scadasim_line = cli_readline();
    if( scadasim_line && scadasim_line[0] != 0 ) {
        scadasim_error = expr_interp(scadasim_line);
        cli_print_interp_error(scadasim_error);
        cli_print_prompt();
    }
  }
}
