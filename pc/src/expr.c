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
#ifndef ARDUINO
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define EXTRA_MATH
#endif //not ARDUINO

#include <errno.h>
#include "expr.h"
#include "parse.h"
#include "var.h"
#include "compat.h"
#include "table.h"


#define FUNC_T		 0
#define FUNC_MS		1
#define FUNC_PI		2
#define FUNC_IF		3
#define FUNC_ABS	 4
#define FUNC_FLOAT	 5
#define FUNC_INT	 6
#define FUNC_FLOOR	 7
#define FUNC_CEIL	8
#define FUNC_ROUND	 9
#define FUNC_RAND	 10
#define FUNC_SERIES 11

#ifdef EXTRA_MATH
#define FUNC_SIN	12
#define FUNC_COS	13
#define FUNC_TAN	14
#define FUNC_ASIN	 15
#define FUNC_ACOS	 16
#define FUNC_ATAN	 17
#define FUNC_LOG	18
#define FUNC_LN	 19
#endif //EXTRA_MATH

const char func_table[] = {
	't'|0x80,
	'm','s'|0x80,
	'p','i'|0x80,
	'i','f'|0x80,
	'a','b','s'|0x80,
	'f','l','o','a','t'|0x80,
	'i','n','t'|0x80,
	'f','l','o','o','r'|0x80,
	'c','e','i','l'|0x80,
	'r','o','u','n','d'|0x80,
	'r','a','n','d'|0x80,
	's','e','r','i','e','s'|0x80,
#ifdef EXTRA_MATH
	's','i','n'|0x80,
	'c','o','s'|0x80,
	't','a','n'|0x80,
	'a','s','i','n'|0x80,
	'a','c','o','s'|0x80,
	'a','t','a','n'|0x80,
	'l','o','g'|0x80,
	'l','n'|0x80,
#endif //EXTRA_MATH
	0x00
};


#define FUNC_START() txtpos=next; ignore_blanks(); if( *txtpos != '(' ) { parse_error = 1; return a; } txtpos++;
#define FUNC_FIRST_ARG(v) FUNC_START(); ignore_blanks(); v=expr1(); if( parse_error ) { return a; }
#define FUNC_NEXT_ARG(v) ignore_blanks(); if( *txtpos != ',' ) { parse_error = 1; return a; } txtpos++; v=expr1(); if( parse_error ) { return a; }
#define FUNC_END() ignore_blanks(); if( *txtpos != ')' ) { parse_error = 1; return a; } txtpos++;
#define FUNC_CHECK_COMMA() ignore_blanks(); if( *txtpos != ',' ) { parse_error=1; return a; } txtpos++;
#define IS_END_CHAR( cp ) ((*(cp)&0x7F) == 0) 

static void func_skip_arg() {
	unsigned int level = 1;
	txtpos++;
	while( 1 ) {
		if( IS_END_CHAR(txtpos) ) {
			parse_error = 1;
			break;
		}
		if( *txtpos == '(' ) {
			level++;
		}
		else if( *txtpos == ')' ) {
			if( level == 1 ) {
				break;
			}
			level--;
			if( level == 0 ) {
				parse_error = 1;
				break;
			}
		}
		else if( *txtpos == ',' ) {
			if( level == 1 ) {
				break;
			}
		}
		txtpos++;
	}
}

static unsigned int func_arg_count() {
	unsigned int level = 1;
	unsigned int count = 0;
	char* tmp = txtpos;
	while( 1 ) {
		if( IS_END_CHAR(tmp) ) {
			parse_error = 1;
			break;
		}
		if( *tmp == '(' ) {
			level++;
		}
		else if( *tmp == ')' ) {
			if( level == 1 ) {
				count++;
				break;
			}
			level--;
			if( level == 0 ) {
				parse_error = 1;
				break;
			}
		}
		else if( *tmp == ',' ) {
			if( level == 1 ) {
				count++;
			}
		}
		tmp++;
	}
	return count;
}

static val_t expr1();

//Precedence: ( ) ! - Constant Function Variable
static val_t expr7() {
	val_t a;
	int idx;
	MAKE_ZERO(a);
	
	ignore_blanks();
	
	//Handle Negations
	if( *txtpos == '-' ) {
		txtpos++;
		a = expr7();
		if( parse_error ) return a;
		if( IS_INT(a) )
			a.i = -a.i;
		else
			a.f = -a.f;
		return a;
	}
	
	//Handle Logical Negations
	if( *txtpos == '!' ) {
		txtpos++;
		a = expr7();
		if( parse_error ) return a;
		if( IS_VAL(a) ) {
			MAKE_ZERO(a);
		}
		else {
			MAKE_ONE(a);
		}
		return a;
	}
	
	//Handle postive constants
	if( parse_positive_number(&a) ) {
		return a;
	}
	
	if(*txtpos == '(') {
		txtpos++;
		a = expr1();
		if( *txtpos != ')' ) {
			parse_error = 1;
		}
		txtpos++;
		return a;
	}
	
	ignore_blanks();
	parse_name();
	
	if( next-txtpos == 0 ) {
		parse_error = 1;
		MAKE_ZERO(a);
		return a;
	}
	
	idx = table_scan(func_table,txtpos,next-txtpos);
	if( idx >= 0 ) {
		val_t b;
		switch(idx) {
			case FUNC_T:
				txtpos = next;
				SET_INT( a, ticks );
				break;
			case FUNC_PI:
				txtpos = next;
				SET_FLOAT( a, M_PI );
				break;
			case FUNC_MS:
				txtpos = next;
				SET_INT(a, compatMillis() );
				break;
			case FUNC_IF:
				FUNC_FIRST_ARG(a);
				if( IS_VAL(a) ) {
					FUNC_NEXT_ARG(a);
					ignore_blanks();
					if( *txtpos == ',' ) {
						txtpos++;
						func_skip_arg();
					}
					if( ! parse_error ) {
						FUNC_END();
					}
				}
				else {
					ignore_blanks();
					if( *txtpos != ',' ) {
						parse_error = 1;
						return a;
					}
					func_skip_arg();
					if( ! parse_error ) {
						if( *txtpos == ')' ) {
							MAKE_ZERO(a);
						}
						else {
							FUNC_NEXT_ARG(a);
						}
						FUNC_END();
					}
				}
				break;
			case FUNC_ABS:
				FUNC_FIRST_ARG(a);
				FUNC_END();
				if( IS_INT(a) ) a.i = abs(a.i);
				else a.f = fabs(a.f);
				break;
			case FUNC_FLOAT:
				FUNC_FIRST_ARG(a);
				FUNC_END();
				MAKE_FLOAT(a);
				break;
			case FUNC_INT:
			case FUNC_FLOOR:
				FUNC_FIRST_ARG(a);
				FUNC_END();
				MAKE_INT(a);
				break;
			case FUNC_CEIL:
				FUNC_FIRST_ARG(a);
				FUNC_END();
				if( IS_FLOAT(a) ) {
					if( a.f > (int)a.f ) {
						MAKE_INT(a);
						a.i++;
					}
					else {
						MAKE_INT(a);
					}
				}
				break;
			case FUNC_ROUND:
				FUNC_FIRST_ARG(a);
				FUNC_END();
				if( IS_FLOAT(a) ) {
					if( (a.f - (int)a.f ) >= 0.5 ) {
						MAKE_INT(a);
						a.i++;
					}
					else {
						MAKE_INT(a);
					}
				}
			break;
			case FUNC_RAND:
				FUNC_FIRST_ARG(a);
				FUNC_NEXT_ARG(b);
				FUNC_END();
				MAKE_INT(a);
				MAKE_INT(b);
				a.i = compatRandom(a.i,b.i);
				break;
			case FUNC_SERIES:
			{
				unsigned int arg_count;
				unsigned int arg_idx;
				FUNC_START();
				arg_count = func_arg_count();
				if( parse_error ) {
				return a;
				}
				arg_idx = 0;
				for( arg_idx=0; arg_idx<arg_count; arg_idx++ ) {
				if( arg_idx == ticks%arg_count )
					a = expr1();
				else {
					func_skip_arg();
				}
				txtpos++;
				}
			}
			break;
#ifdef EXTRA_MATH
			case FUNC_SIN:
				FUNC_FIRST_ARG(a);
				FUNC_END();
				MAKE_FLOAT(a);
				a.f = sin(a.f);
				break;
			case FUNC_COS:
				FUNC_FIRST_ARG(a);
				FUNC_END();
				MAKE_FLOAT(a);
				a.f = cos(a.f);
				break;
			case FUNC_TAN:
				FUNC_FIRST_ARG(a);
				FUNC_END();
				MAKE_FLOAT(a);
				a.f = tan(a.f);
				break;
			case FUNC_ASIN:
				FUNC_FIRST_ARG(a);
				FUNC_END();
				MAKE_FLOAT(a);
				a.f = asin(a.f);
				break;
			case FUNC_ACOS:
				FUNC_FIRST_ARG(a);
				FUNC_END();
				MAKE_FLOAT(a);
				a.f = acos(a.f);
				break;
			case FUNC_ATAN:
				FUNC_FIRST_ARG(a);
				FUNC_END();
				MAKE_FLOAT(a);
				a.f = atan(a.f);
				break;
			case FUNC_LOG:
				FUNC_FIRST_ARG(a);
				FUNC_END();
				MAKE_FLOAT(a);
				a.f = log10f(a.f);
				break;
			case FUNC_LN:
				FUNC_FIRST_ARG(a);
				FUNC_END();
				MAKE_FLOAT(a);
				a.f = logf(a.f);
				break;
#endif //EXTRA_MATH
			default:
				parse_error = 1;
		}
		return a;
	}

	//Assume that this is a variable
	//get_var will create it if is does not already exist
	var_t *v = get_var(txtpos,next-txtpos);
	if( !v ) {
		parse_error = 1;
		return a;
	}
	a = v->value;
	txtpos = next;
	return a;
}

//Precedence: ** * / %
static val_t expr6() {
	val_t a;
	
	a = expr7();
	if( parse_error ) return a;
	
	ignore_blanks();
	while( 1 ) {
		if( *txtpos == '*' && *(txtpos+1) == '*' ) {
			val_t b;
			MAKE_FLOAT(a);
			MAKE_FLOAT(b);
			a.f = pow(a.f,b.f);
		}
		else if( *txtpos == '*' ) {
			val_t b;
			txtpos++;
			b = expr7();
			if( parse_error ) return a;
			if( IS_FLOAT(a) || IS_FLOAT(b) ) {
				MAKE_FLOAT(a);
				MAKE_FLOAT(b);
				a.f = a.f * b.f;
			}
			else {
				a.i = a.i * b.i;
			}
		}
		else if( *txtpos == '/' ) {
			val_t b;
			char floor = 0;
			txtpos++;
			if( *txtpos == '/' ) {
				floor = 1;
				txtpos++;
			}
			b = expr7();
			if( parse_error ) return a;
			if( IS_FLOAT(a) || IS_FLOAT(b) ) {
				MAKE_FLOAT(a);
				MAKE_FLOAT(b);
				if( b.f == 0 ) {
					parse_error = 1;
					return a;
				}
				a.f = a.f / b.f;
			}
			else {
				if( b.i == 0 ) {
					parse_error = 1;
					return a;
				}
				a.i = a.i / b.i;
			}
			if( floor ) {
				MAKE_INT(a);
			}
		}
		else if( *txtpos == '%' ) {
			val_t b;
			txtpos++;
			b = expr7();
			if( parse_error ) return a;
			MAKE_INT(a);
			MAKE_INT(b);
			if( b.i == 0 ) {
				parse_error = 1;
				return a;
			}
			a.i = a.i % b.i;
		}
		else {
			return a;
		}
	}
}

//Precedence: + -
static val_t expr5() {
	val_t a;
	
	a = expr6();
	if( parse_error ) return a;
	
	ignore_blanks();
	while( 1 ) {
		if( *txtpos == '+' ) {
			val_t b;
			txtpos++;
			b = expr6();
			if( parse_error ) return a;
			if( IS_FLOAT(a) || IS_FLOAT(b) ) {
				MAKE_FLOAT(a);
				MAKE_FLOAT(b);
				a.f = a.f + b.f;
			}
			else {
				a.i = a.i + b.i;
			}
		}
		else if( *txtpos == '-' ) {
			val_t b;
			txtpos++;
			b = expr6();
			if( parse_error ) return a;
			if( IS_FLOAT(a) || IS_FLOAT(b) ) {
				MAKE_FLOAT(a);
				MAKE_FLOAT(b);
				a.f = a.f - b.f;
			}
			else {
				a.i = a.i - b.i;
			}
		}
		else {
			return a;
		}
	}
}

//Precedence: << >>
static val_t expr4() {
	val_t a;
	
	a = expr5();
	if( parse_error ) return a;
	
	ignore_blanks();
	while( 1 ) {
		if( *txtpos == '<' && *(txtpos+1) == '<' ) {
			val_t b;
			txtpos = txtpos + 2;
			b = expr5();
			if( parse_error ) return a;
			MAKE_INT(a);
			MAKE_INT(b);
			a.i = a.i << b.i;
		}
		if( *txtpos == '>' && *(txtpos+1) == '>' ) {
			val_t b;
			txtpos = txtpos + 2;
			b = expr5();
			if( parse_error ) return a;
			MAKE_INT(a);
			MAKE_INT(b);
			a.i = a.i >> b.i;
		}
		else {
			return a;
		}
	}
}

//Precedence: == != < <= > >=
static val_t expr3() {
	val_t a;
	
	a = expr4();
	if( parse_error ) return a;
	
	ignore_blanks();
	while( 1 ) {
		if( *txtpos == '=' && *(txtpos+1) == '=' ) {
			val_t b;
			txtpos = txtpos + 2;
			b = expr4();
			if( parse_error ) return a;
			
			if( IS_FLOAT(a) || IS_FLOAT(b) ) {
				MAKE_FLOAT(a);
				MAKE_FLOAT(b);
				if( a.f == b.f ) { MAKE_ONE(a); }
				else { MAKE_ZERO(a); }
			}
			else {
				if( a.i == b.i ) { MAKE_ONE(a); }
				else { MAKE_ZERO(a); }
			}
		}
		else if( *txtpos == '!' && *(txtpos+1) == '=' ) {
			val_t b;
			txtpos = txtpos + 2;
			b = expr4();
			if( parse_error ) return a;
			
			if( IS_FLOAT(a) || IS_FLOAT(b) ) {
				MAKE_FLOAT(a);
				MAKE_FLOAT(b);
				if( a.f != b.f ) { MAKE_ONE(a); }
				else { MAKE_ZERO(a); }
			}
			else {
				if( a.i != b.i ) { MAKE_ONE(a); }
				else { MAKE_ZERO(a); }
			}
		}
	else if( *txtpos == '<' && *(txtpos+1) == '=' ) {
		val_t b;
		txtpos = txtpos + 2;
		b = expr4();
		if( parse_error ) return a;
		
		if( IS_FLOAT(a) || IS_FLOAT(b) ) {
		MAKE_FLOAT(a);
		MAKE_FLOAT(b);
		if( a.f <= b.f ) { MAKE_ONE(a); }
		else { MAKE_ZERO(a); }
		}
		else {
		if( a.i <= b.i ) { MAKE_ONE(a); }
		else { MAKE_ZERO(a); }
		}
	}
	else if( *txtpos == '<' && *(txtpos+1) != '<' ) {
		val_t b;
		txtpos++;
		b = expr4();
		if( parse_error ) return a;
		
		if( IS_FLOAT(a) || IS_FLOAT(b) ) {
		MAKE_FLOAT(a);
		MAKE_FLOAT(b);
		if( a.f < b.f ) { MAKE_ONE(a); }
		else { MAKE_ZERO(a); }
		}
		else {
		if( a.i < b.i ) { MAKE_ONE(a); }
		else { MAKE_ZERO(a); }
		}
	}
	else if( *txtpos == '>' && *(txtpos+1) == '=' ) {
		val_t b;
		txtpos = txtpos + 2;
		b = expr4();
		if( parse_error ) return a;
		
		if( IS_FLOAT(a) || IS_FLOAT(b) ) {
		MAKE_FLOAT(a);
		MAKE_FLOAT(b);
		if( a.f >= b.f ) { MAKE_ONE(a); }
		else { MAKE_ZERO(a); }
		}
		else {
		if( a.i >= b.i ) { MAKE_ONE(a); }
		else { MAKE_ZERO(a); }
		}
	}
	else if( *txtpos == '>' && *(txtpos+1) != '>' ) {
		val_t b;
		txtpos++;
		b = expr4();
		if( parse_error ) return a;
		
		if( IS_FLOAT(a) || IS_FLOAT(b) ) {
		MAKE_FLOAT(a);
		MAKE_FLOAT(b);
		if( a.f > b.f ) { MAKE_ONE(a); }
		else { MAKE_ZERO(a); }
		}
		else {
		if( a.i > b.i ) { MAKE_ONE(a); }
		else { MAKE_ZERO(a); }
		}
	}
		else {
			return a;
		}
	}
}

//Precedence: | ^ &
static val_t expr2() {
	val_t a;

	a = expr3();
	if( parse_error ) return a;
	
	ignore_blanks();
	while( 1 ) {
		if( *txtpos == '|' && *(txtpos+1) != '|' ) {
			val_t b;
			txtpos++;
			b = expr3();
			if( parse_error ) return a;
			MAKE_INT(a);
			MAKE_INT(b);
			a.i = a.i | b.i;
		}
	else if( *txtpos == '^' ) {
		val_t b;
		txtpos++;
		b = expr3();
		if( parse_error ) return a;
		MAKE_INT(a);
		MAKE_INT(b);
		a.i = a.i ^ b.i;
	}
	else if( *txtpos == '&' && *(txtpos+1) != '&' ) {
		val_t b;
		txtpos++;
		b = expr3();
		if( parse_error ) return a;
		MAKE_INT(a);
		MAKE_INT(b);
		a.i = a.i & b.i;
	}
		else {
			return a;
		}
	}
}

//Precedence: && ||
static val_t expr1() {
	val_t a;
	
	a = expr2();
	if( parse_error ) return a;
	
	ignore_blanks();
	while( 1 ) {
		if( *txtpos == '|' && *(txtpos+1) == '|' ) {
			val_t b;
			txtpos = txtpos + 2;
			b = expr2();
			if( parse_error ) return a;
			
			if( IS_VAL(a) || IS_VAL(b) ) {
				MAKE_ONE(a);
			}
			else {
				MAKE_ZERO(a);
			}
		}
	else if( *txtpos == '&' && *(txtpos+1) == '&' ) {
		val_t b;
		txtpos = txtpos + 2;
		b = expr2();
		if( parse_error ) return a;;
		if( IS_VAL(a) && IS_VAL(b) ) {
		MAKE_ONE(a);
		}
		else {
		MAKE_ZERO(a);
		}
	}
		else {
			return a;
		}
	}
}


//Uses txtpos.  Set global variable before calling.
int expr_eval(val_t *a) {
	ignore_blanks();
	*a = expr1();
	if( parse_error )
		return 0;
	return 1;
}
