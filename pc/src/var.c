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
#include "compat.h"
#include "parse.h"
#include "expr.h"
#include "cli.h"
#include "table.h"

#include <stdio.h>
#include <string.h>

static char names[NAMESMAX];
static char exprs[EXPRSMAX];
var_t vars[VARSMAX];
unsigned int ticks;
unsigned int last_tickmillis;
char newVars;

void varBegin() {
	unsigned int i;
	table_init(names);
	table_init(exprs);
	for( i=0; i<VARSMAX; i++ ) {
		vars[i].name = 0;
		vars[i].expr = 0;
		vars[i].value.type = VAL_NONE;
		vars[i].pnttype = PNT_NONE;
	}
	ticks = 0;
	last_tickmillis = 0;
	newVars = 0;
}

void varProcess() {
	unsigned int millis = compatMillis();
	val_t a;
	var_t *v;
	if( millis - last_tickmillis > TICKDELAY ) {
		last_tickmillis = millis;
		ticks++;
		v = vars;
		while( v < vars+VARSMAX ) {
			if( v->value.type == VAL_NONE ) {
				break;
			}
			if( v->expr != 0 ) {
				txtpos = v->expr;
				MAKE_ZERO(a);
				if( ! expr_eval(&a) ) {
					cli_print_eval_error(v,txtpos-v->expr+1);
				} else {
					v->value = a;
				}
			}
			v++;
		}
		newVars = 1;
	}
	else {
		newVars = 0;
	}
}

int set_expr(var_t* var, char* expr, unsigned int len) {
	if( var->expr ) {
		unsigned int eoff;
		unsigned int i;
		eoff = table_del(var->expr);
		for( i=0; i<VARSMAX; i++ ) {
			if( vars[i].expr > var->expr )
				vars[i].expr = vars[i].expr-eoff;
		}
	}
	if( expr ) {
		var->expr = table_add(exprs,EXPRSMAX,expr,len,1);
		if( ! var->expr ) {
			del_var(var);
			return 0;
		}
	} else {
		var->expr = 0;
	}
	return 1;
}

var_t* make_var(char* name, unsigned int len) {
	var_t * v  = vars;
	while( v < vars+VARSMAX ) {
		if( v->value.type == VAL_NONE ) {
			v->name = table_add(names,NAMESMAX,name,len,0);
			if( v->name != 0 ) {
				v->expr = 0;
				MAKE_ZERO(v->value);
				return v;
			}
			break;
		}
		v++;
	}
	return 0;
}

var_t* get_var(char* name, unsigned int len) {
	int var_idx;
	var_idx = table_scan(names,name,len);
	if( var_idx < 0 ) {
		return 0;
	}
	return vars+var_idx;
}

void del_var(var_t* v) {
	unsigned int i = 0;
	unsigned int eoff = 0;
	unsigned int noff = 0;
	if( v == 0 ) {
		return;
	}
	
	if( v->name )
		noff = table_del(v->name);
	if( v->expr )
		eoff = table_del(v->expr);

	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].expr > v->expr )
			vars[i].expr = vars[i].expr-eoff;
		if( vars[i].name > v->name )
			vars[i].name = vars[i].name-noff;
	}
	
	while( v < (vars+VARSMAX-1) ) {
		*v = *(v+1);
		v++;
	}
	
	v->value.type = VAL_NONE;
	v->name = 0;
	v->expr = 0;
	v->pnttype = PNT_NONE;
}

