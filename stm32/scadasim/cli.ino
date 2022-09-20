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
#include "cli.h"
#include "var.h"
#include "expr.h"
#include "val.h"
#include "modbus.h"

static char cli_line[LINEMAX];
static unsigned int cli_linelen;
static char cli_lcdline[21];

static void print_varval(const var_t *var) {
	if( var == 0 || var->name == 0 )
		return;
  Serial.print(var->name);
  Serial.print("=");
	cli_print_val(var->value);
}

static void  print_varline(const var_t* var) {
	if( var == 0 || var->name == 0 )
		return;
	if( var->expr == 0 && var->pnttype == PNT_NONE ) {
		print_varval(var);
		return;
	}
  Serial.print(var->name);
	if( var->expr !=  0 ) {
    Serial.print(" =");
    Serial.print(var->expr);
	}
	if( var->pnttype != PNT_NONE ) {
		if( var->pnttype == PNT_DO ) {
      Serial.print(" :do ");
      Serial.print(var->pntaddr); 
		}
		else if( var->pnttype == PNT_DI ) {
      Serial.print(" :di ");
      Serial.print(var->pntaddr); 
    }
		else if( var->pnttype == PNT_AO ) {
      Serial.print(" :ao ");
      Serial.print(var->pntaddr);
		}
		else if( var->pnttype == PNT_AO_SCALED ) {
      Serial.print(" :ao scaled ");
      Serial.print(var->pntaddr);
      Serial.print(" ");
      Serial.print(var->pntmin,3);
      Serial.print(" ");
      Serial.print(var->pntmax,3);
		}
		else if( var->pnttype == PNT_AI ) {
      Serial.print(" :ai ");
      Serial.print(var->pntaddr);
    }
		else if( var->pnttype == PNT_AI_SCALED ) {
      Serial.print(" :ai scaled ");
      Serial.print(var->pntaddr);
      Serial.print(" ");
      Serial.print(var->pntmin,3);
      Serial.print(" ");
      Serial.print(var->pntmax,3);
		}
	}
	Serial.print("\n");
}


void cli_init() {
	cli_linelen = 0;
}

char* cli_readline() {
  int c;
  while( Serial.available() ) {
    c = Serial.read();
    Serial.print((char)c);
    if( c == '\r' || c == '\n' ) {
      if( cli_linelen ==  0 ) {
        cli_print_prompt();
        continue;
      }
      Serial.println();
      cli_line[cli_linelen] = 0;
      cli_linelen = 0;
      return cli_line;
    }
    //Handle DEL character
    if( c == 0x7F ) {
      cli_linelen--;
      continue;
    }
    cli_line[cli_linelen++] = (char)c;
  }
  return 0;
}

void cli_print_prompt() {
  Serial.print(">");
}

void cli_print_var_by_name(char* varname, unsigned int len) {
	var_t *v = get_var(varname,len);
	if( v != 0 ) {
		print_varval(v);
	}
}

void cli_print_var_by_index(unsigned int idx) {
	var_t *v;
	if( idx >= VARSMAX )
		return;
	v = &vars[idx];
	print_varval(v);
}

void cli_print_interp_error(int error) {
	if( error == 0 )
		return;
	else if( error < 0 )
    Serial.println("  Internal Error\n");
	else {
    int i;
    for( i=0; i<error; i++ ) {
      Serial.print(" ");
    }
    Serial.println("^ Error Here");
	}
}

void cli_print_eval_error(unsigned int idx, int error) {
	if( error == 0 || vars[idx].name == 0 || vars[idx].expr == 0 )
		return;
  Serial.print(vars[idx].name);
  Serial.print(" =");
	Serial.println(vars[idx].expr);
	if( error < 0 )
		Serial.println("  Internal Error\n");
	else {
    error = error+strlen(vars[idx].name)+2;
    int i;
    for( i=0; i<error; i++ ) {
      Serial.print(" ");
    }
    Serial.println("^ Error Here");
	}
}

void cli_print_list() {
	unsigned int i;
	for( i=0; i<VARSMAX; i++ ) {
		print_varline(&vars[i]);
	}
  Serial.print("modbus ");
  Serial.println(modbus_address);
}

void cli_print_state() {
	unsigned int i;
	for( i=0; i<VARSMAX; i++ ) {
		print_varval(&vars[i]);
	}
}

void cli_print_val(val_t v) {
	if( v.type == VAL_INT )
    Serial.println(v.i);
	else if( v.type == VAL_FLOAT )
    Serial.println(v.f,3);
	else 
    Serial.println("?");
}

char* cli_lcd_line(unsigned int idx) {
  if( vars[idx].value.type == VAL_INT ) {
    snprintf(cli_lcdline,20,"%s=%d",vars[idx].name,vars[idx].value.i);
  }
  else if( vars[idx].value.type == VAL_FLOAT ) {
    snprintf(cli_lcdline,20,"%s=%0.3f",vars[idx].name,vars[idx].value.f);
  }
  else {
    snprintf(cli_lcdline,20,"%s=?",vars[idx].name);
  }
  cli_lcdline[20] = 0;
  return cli_lcdline;
}
