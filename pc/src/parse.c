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
#define __PARSE_C__
#include "parse.h"

#include <errno.h>
#include <stdlib.h>

char* txtpos;
char* next;
uint8_t parse_error;

void ignore_blanks() {
	while(*txtpos == ' ' || *txtpos == '\t')
		txtpos++;
}

void parse_name() {
	next = txtpos;
	if( (*next >= 'a' && *next <= 'z') || (*next >= 'A' && *next <= 'Z') || *next == '_' ) {
		while( (*next >= 'a' && *next <= 'z') || (*next >= 'A' && *next <= 'Z') || (*next >= '0' && *next <= '9') || *next == '_') {
			next++;
		}
	}
}

int parse_positive_number(val_t* a) {
	if( *txtpos == '.' || (*txtpos >= '0' && *txtpos <= '9') ) {
		int base = 10;
		//Check if constant is specified in hex
		if( *txtpos == '0' && (*(txtpos+1) == 'x' || *(txtpos+1) == 'X') ) {
			base = 16;
			txtpos = txtpos + 2;
		}
		//Check if constant is specified in binary
		else if( *txtpos == '0' && (*(txtpos+1) == 'b' || *(txtpos+1) == 'B') ) {
			base = 2;
			txtpos = txtpos + 2;
		}
		//Check to see if constant has a decimal point
		next = txtpos;
		while( *next >= '0' && *next <= '9' ) {
			next++;
		}
		if( *next == '.' ) {
			//Constant is a float
			a->type = VAL_FLOAT;
			a->f = 0;
			if( base != 10 )
				parse_error = 1;
			if( !parse_error ) {
				errno = 0;
				a->f = strtof(txtpos,&next);
				parse_error = errno;
			}
			if( !parse_error )
				txtpos = next;
		}
		else {
			//Constant is an integer
			errno = 0;
			a->type = VAL_INT;
			a->i = (int)strtol(txtpos,&next,base);
			parse_error = errno;
			if( !parse_error )
				txtpos = next;
		}
		return 1;
	}
	return 0;
}

unsigned int parse_unsigned_int() {
	int r = -1;
	int base = 10;
	if( ! (*txtpos >= '0' && *txtpos <= '9') ) {
		parse_error = 1;
		return r;
	}
	//Check if constant is specified in hex
	if( *txtpos == '0' && (*(txtpos+1) == 'x' || *(txtpos+1) == 'X') ) {
		base = 16;
		txtpos = txtpos + 2;
	}
	//Check if constant is specified in binary
	else if( *txtpos == '0' && (*(txtpos+1) == 'b' || *(txtpos+1) == 'B') ) {
		base = 2;
		txtpos = txtpos + 2;
	}
	//Constant is an integer
	errno = 0;
	r = (int)strtol(txtpos,&next,base);
	parse_error = errno;
	if( !parse_error )
		txtpos = next;
	return r;
}

float parse_float() {
	float r;
	if( *txtpos < '0' && *txtpos > '9' && *txtpos != '-' ) {
		parse_error = 1;
		return r;
	} 
	errno = 0;
	r = strtof(txtpos,&next);
	parse_error = errno;
	if( !parse_error )
		txtpos = next;
	return r;
}
