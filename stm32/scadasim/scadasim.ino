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
#include "cli.h"
#include "var.h"
#include "LiquidCrystal.h"

#define TICKINTERVAL    500
#define DISPLAYINTERVAL 2000

static char* scadasim_line;
static int scadasim_error;
static unsigned long last_tick;
static unsigned long last_display;
static unsigned int next_display_idx;
LiquidCrystal lcd = LiquidCrystal(PB9,PA1,PA4,PA5,PA6,PA7,PB0,PB1,PB10,PB11);

void setup() {
  //Fix r/w pin for LCD (data pins used can't handled 5V input)
  pinMode(PA0,OUTPUT);
  digitalWrite(PA0,LOW);

  //Initialize the LCD
  lcd.begin(20, 4);


  //Initialize the Serial Ports
  Serial.begin(9600);
  Serial2.begin(9600);

  //Seed the random number generator
  randomSeed(analogRead(0));
 
  //Initalize the other subsystems
  var_init();
  cli_init();
  resetModbus();

  cli_print_prompt();
  last_tick = millis();
  last_display = millis();
  next_display_idx = 0;
}

void update_display() {
  unsigned int idx = next_display_idx;
  unsigned int count = 0;
  lcd.clear();
  do{
    if( vars[idx].name != 0 and vars[idx].name[0] != '_' ) {
      lcd.setCursor(0,count++);
      lcd.print(cli_lcd_line(idx));
    }
    idx++;
    if( idx == VARSMAX && count == 0 )
      idx = 0;
  } while( idx < VARSMAX && idx != next_display_idx && count < 4 );
  next_display_idx = idx % VARSMAX;
}

void loop() {
  //Check to see if we need to tick the simulation
  if( millis() - last_tick >= TICKINTERVAL ) {
    last_tick = last_tick + TICKINTERVAL;
    expr_tick();
  }

  //Process Console port input
  scadasim_line = cli_readline();
  if( scadasim_line && scadasim_line[0] != 0 ) {
      scadasim_error = expr_interp(scadasim_line);
      cli_print_interp_error(scadasim_error);
      cli_print_prompt();
  }

  //Process Data port input
  processModbus();

  //Handle LCD update
  if( millis() - last_display >= DISPLAYINTERVAL ) {
    update_display();
    last_display = last_display + DISPLAYINTERVAL;
  }
}
