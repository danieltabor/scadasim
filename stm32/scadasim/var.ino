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
#include <string.h>
#define __VAR_C__
#include "var.h"

#include <stdio.h>

static char names[NAMESMAX];
static unsigned int nameslen;
static char exprs[EXPRSMAX];
static unsigned int exprslen;
var_t vars[VARSMAX];
unsigned int ticks;

void var_init() {
	unsigned int i;
	nameslen = 0;
	exprslen = 0;
	for( i=0; i<VARSMAX; i++ ) {
		vars[i].value.type = VAL_NONE;
		vars[i].name = 0;
		vars[i].expr = 0;
		vars[i].pnttype = PNT_NONE;
	}
	ticks = 0;
}

char* add_name(char* name, unsigned int len) {
	char* n;
	if( (nameslen + len + 1) > NAMESMAX ) {
		n = 0;
	}
	else {
		n = names+nameslen;
		memcpy(names+nameslen,name,len);
		names[nameslen+len] = 0;
		nameslen = nameslen + len + 1;
	}
	return n;
}

void del_name(char* name) {
	unsigned int len = strlen(name) + 1;
	unsigned int i;
	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].name > name ) {
			vars[i].name = vars[i].name - len;
		}
	}
	for( i=(name-names)+len; i<NAMESMAX; i++ ) {
		names[i-len] = names[i];
	}
}

char* add_expr(char* txtpos) {
	char* start = exprs+exprslen; 
	char* e = start;
	while( e < exprs+EXPRSMAX ) {
		if( *txtpos == ':' || *txtpos == '\n' || *txtpos == 0 ) {
			*e++ = 0;
			txtpos++;
			break;
		}
		else if( *txtpos == ' ' || *txtpos == '\t' ) {
			txtpos++;
		}
		else {
			*e++ = *txtpos++;
		}
	}
	if( e >= exprs+EXPRSMAX ) {
		start = 0;
	}
	else {
		exprslen = e - exprs;
	}
	return start;
}

void del_expr(char* e) {
	unsigned int len = strlen(e) + 1;
	unsigned int i;
	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].expr > e ) {
			vars[i].expr = vars[i].expr - len;
		}
	}
	for( i=(e-exprs)+len; i<EXPRSMAX; i++ ) {
		exprs[i-len] = exprs[i];
	}
}

var_t* get_var(char* name, unsigned int len) {
	unsigned int i;
	var_t *v = 0;
	if( len == 0 ) {
		len = strlen(name);
	}
	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].value.type == VAL_NONE || strncmp(vars[i].name,name,len) == 0 ) {
			v = &vars[i];
			break;
		}
	}
	if( v && v->value.type == VAL_NONE ) {
		v->name = add_name(name,len);
		if( v->name == 0 ) {
			v = 0;
		}
		else {
			v->value.type = VAL_INT;
			v->value.i = 0;
			v->expr = 0;
		}
	}
	return v;
}

void del_var(var_t* v) {
	unsigned int i;
	if( v == 0 )
		return;
	v->value.type = VAL_NONE;
	if( v->name )
		del_name(v->name);
	if( v->expr )
		del_expr(v->expr);
	for( i=(v-vars)+1; i<VARSMAX; i++ ) {
		vars[i-1] = vars[i];
	}
	vars[VARSMAX-1].value.type = VAL_NONE;
	vars[VARSMAX-1].name = 0;
	vars[VARSMAX-1].expr = 0;
	vars[VARSMAX-1].pnttype = PNT_NONE;
}

unsigned int var_count() {
	unsigned int i;
	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].value.type != VAL_NONE ) {
			break;
		}
	}
	return i;
}

const char* var_name(unsigned int idx) {
	return vars[idx].name;
}

int var_index(const char* name) {
	unsigned int i;
	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].value.type == VAL_NONE )
			break;
		else if( vars[i].name && strcmp(vars[i].name,name) == 0 )
			return i;
	}
	return -1;
}

val_t var_val(unsigned int idx) {
	return vars[idx].value;
}
