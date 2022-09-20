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
#ifndef __VAR_H__
#define __VAR_H__

#define VARSMAX    100
#define NAMESMAX  1024
#define EXPRSMAX  5120
#define TICKDELAY  250

#include "val.h"

#define PNT_NONE      0
#define PNT_DO        1
#define PNT_DI        2
#define PNT_AO        3
#define PNT_AO_SCALED 4
#define PNT_AI        5
#define PNT_AI_SCALED 6

typedef struct {
	val_t value;
	char* name;
	char* expr;
	unsigned char pnttype;
	unsigned int pntaddr;
	float pntmin;
	float pntmax;
	#ifdef IEC61850
	void *iec61850_value;
	void *iec61850_timestamp;
	void *iec61850_ctrl;
	#endif
} var_t;

#ifndef __VAR_C__
extern var_t vars[VARSMAX];
extern unsigned int ticks;
extern char newVars;
#endif 

void varBegin();
void varProcess();

int set_expr(var_t* var, char* expr, unsigned int len);
var_t* make_var(char* name, unsigned int len);
var_t* get_var(char* name, unsigned int len);
void del_var(var_t* v) ;

#endif //__VAR_H__