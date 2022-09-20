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
#ifndef __VAL_H__
#define __VAL_H__

#define VAL_NONE  0
#define VAL_FLOAT 1
#define VAL_INT   2

typedef struct {
	unsigned char type;
	union {
		float f;
		int   i;
	};
} val_t;

#define MAKE_FLOAT( v ) if( v.type == VAL_INT ) { v.type = VAL_FLOAT; v.f = (float)v.i; }
#define MAKE_INT( v ) if( v.type == VAL_FLOAT ) { v.type = VAL_INT; v.i = (int)v.f; }
#define MAKE_ONE( v ) v.type = VAL_INT; v.i = 1;
#define MAKE_ZERO( v ) v.type = VAL_INT; v.i = 0;
#define IS_VAL( v ) ((v.type == VAL_INT && v.i != 0) || (v.type == VAL_FLOAT && v.f != 0.0))

#endif //__VAL_H__
