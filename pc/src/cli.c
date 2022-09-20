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
#include <stdio.h>
#include <string.h>

#include "cli.h"
#include "var.h"
#include "val.h"
#include "table.h"
#include "compat.h"
#include "display.h"

#ifdef MODBUS
#include "modbus.h"
#endif 

#ifdef MODBUSTCP
#include "modbustcp.h"
#endif

#ifdef IEC61850
#include "iec61850.h"
#endif

static char line[LINEMAX];
static unsigned int linelen;

static char output[LINEMAX];
static unsigned int outlen;

#ifndef ARDUINO
static FILE* loadfp = 0;
static FILE* savefp = 0;
#endif

#define append_printf(...) outlen = outlen + snprintf(output+outlen,LINEMAX-outlen,__VA_ARGS__)

int append_table_entry(char* entry) {
	char* next = table_next(entry);
	int count = 0;
	while( entry != next ) {
		output[outlen] = *entry&0x7F;
		count++;
		if( ! output[outlen] )
			break;
		outlen++;
		entry++;
	}
	output[outlen] = 0;
	return count;
}

void append_val(val_t v) {
	if( v.type == VAL_INT )
		append_printf("%d",v.i);
	else if( v.type == VAL_FLOAT )
		append_printf("%0.3f",v.f);
	else 
		append_printf("?");
}

void  append_varline(const var_t* var) {
	if( var == 0 || var->name == 0 )
		return;
	append_table_entry(var->name);
	append_printf(" =");
	if( var->expr !=  0 ) {
		append_table_entry(var->expr);
	}
	else {
		append_val(var->value);
	}
	if( var->pnttype != PNT_NONE ) {
		if( var->pnttype == PNT_DO )
			append_printf(" :do %d",var->pntaddr);
		else if( var->pnttype == PNT_DI )
			append_printf(" :di %d",var->pntaddr);
		else if( var->pnttype == PNT_AO )
			append_printf(" :ao %d",var->pntaddr);
		else if( var->pnttype == PNT_AO_SCALED )
			append_printf(" :ao scaled %d %f %f",var->pntaddr,var->pntmin,var->pntmax);
		else if( var->pnttype == PNT_AI )
			append_printf(" :ai %d",var->pntaddr);
		else if( var->pnttype == PNT_AI_SCALED )
			append_printf(" :ai scaled %d %f %f",var->pntaddr,var->pntmin,var->pntmax);
	}
	append_printf("\n");
}


void cli_init() {
	linelen = 0;
}

char* cli_readline() {
	int c;
	while( linelen < LINEMAX ) {
#ifndef ARDUINO
		if( loadfp ) {
			c = 0;
			if( ! fread(&c,1,1,loadfp) ) {
				fclose(loadfp);
				loadfp = 0;
				c = -1;
			}
		}
		else
#endif 
		c = consoleIn();
		if( c < 0 ) {
			return 0;
		}
		if( c == 0x7F || c == 0x08 ) {
			consoleOut((char)c);
			if( linelen ) {
				linelen--;
			}
		} else if( c == '\n' || c == '\r' ) {
			if( linelen ) {
				consoleOut((char)c);
				line[linelen] = 0;
				do {
					linelen--;
					if( line[linelen] == '#' ) {
						line[linelen] = 0;
					}
				} while( linelen > 0 );
				return line;
			}
		} else {
			consoleOut((char)c);
			line[linelen++] = (char)c;
		}
	}
	linelen = 0;
	return 0;
}

void cli_printline() {
	char* c = output;
	#ifndef ARDUINO
	if( savefp ) {
		fwrite(output,1,outlen,savefp);
	}
	#endif
	while( *c ) {
		consoleOut(*c);
		c++;
	}
	outlen = 0;
}

void cli_print_prompt() {
	consoleOut('>');
}

void cli_print_interp_error(int error) {
	if( error == 0 )
		return;
	else if( error < 0 )
		append_printf("  Internal Error\n");
	else
		append_printf(" %*s Error Here\n",error,"^");
	cli_printline();
}

void cli_print_eval_error(var_t *v, int error) {
	int len;
	if( error == 0 || v == 0 || vars->name == 0 || vars->expr == 0 )
		return;
	len = append_table_entry(v->name) + 2;
	append_printf(" =");
	append_table_entry(v->expr);
	append_printf("\n");
	if( error < 0 )
		append_printf("  Internal Error\n");
	else
		append_printf("%*s Error Here\n",len+error,"^");
	cli_printline();
}

void cli_print_list() {
	var_t* v = vars;
	while( v < vars+VARSMAX ) {
		if( v->value.type == VAL_NONE )
			break;
		append_varline(v);
		cli_printline();
		v++;
	}
	#ifdef MODBUS
		append_printf("modbus %d\n",modbus_address);
	#endif
	#ifdef MODBUSTCP
		if( modbustcp_port != 0 ) {
			append_printf("modbustcp %d\n",modbustcp_port);
		}
	#endif
	#ifdef IEC61850
		if( iec61850_serv_name[0] ) {
			append_printf("iec61850 %s %d\n",iec61850_serv_name,iec61850_serv_port);
		}
		if( iec61850_goose_digital_eth[0] ) {
			append_printf("gsed %s %d %02x:%02x:%02x:%02x:%02x:%02x\n",iec61850_goose_digital_eth,iec61850_goose_digital_appid,
				iec61850_goose_digital_dst[0]&0xFF,iec61850_goose_digital_dst[1]&0xFF,iec61850_goose_digital_dst[2]&0xFF,
				iec61850_goose_digital_dst[3]&0xFF,iec61850_goose_digital_dst[4]&0xFF,iec61850_goose_digital_dst[5]&0xFF);
		}
		if( iec61850_goose_analog_eth[0] ) {
			append_printf("gsea %s %d %02x:%02x:%02x:%02x:%02x:%02x\n",iec61850_goose_analog_eth,iec61850_goose_analog_appid,
				iec61850_goose_analog_dst[0]&0xFF,iec61850_goose_analog_dst[1]&0xFF,iec61850_goose_analog_dst[2]&0xFF,
				iec61850_goose_analog_dst[3]&0xFF,iec61850_goose_analog_dst[4]&0xFF,iec61850_goose_analog_dst[5]&0xFF);
		}
		if( iec61850_icd_path[0] ) {
			append_printf("icd %s\n",iec61850_icd_path);
		}			
	#endif
	cli_printline();
	if( strlen(gfxpath) ) {
		append_printf("gfx %s\n",gfxpath);
		cli_printline();
	}
}

void cli_print_state() {
	var_t* v = vars;
	while( v < vars+VARSMAX ) {
		if( v->value.type == VAL_NONE )
			break;
		if( v->name && v->name[0] != '_' ) {
			append_table_entry(v->name);
			append_printf(":");
			append_val(v->value);
			append_printf("\n");
			cli_printline();
		}
		v++;
	}
}

void cli_print_val(val_t v) {
	append_val(v);
	append_printf("\n");
	cli_printline();
}

#ifndef ARDUINO
void cli_start_save(char* path) {
	savefp = fopen(path,"wb");
	cli_print_list();
	fclose(savefp);
	savefp = 0;
}

void cli_start_load(char* path) {
	loadfp = fopen(path,"rb");
}
#endif //ARDUINO