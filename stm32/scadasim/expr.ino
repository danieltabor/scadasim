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
#else
#include "modbus.h"
#endif

#include <errno.h>
#include "expr.h"
#include "var.h"
#include "cli.h"

#ifdef MINI
#include "indicators.h"
#endif

static char* txtpos;
static char* next;
static uint8_t error;

static void ignore_blanks(void) {
	while(*txtpos == ' ' || *txtpos == '\t')
		txtpos++;
}

static void parse_name() {
	next = txtpos;
	if( (*next >= 'a' && *next <= 'z') || (*next >= 'A' && *next <= 'Z') || *next == '_' ) {
		while( (*next >= 'a' && *next <= 'z') || (*next >= 'A' && *next <= 'Z') || (*next >= '0' && *next <= '9') || *next == '_') {
			next++;
		}
	}
}

static unsigned int parse_unsigned_int() {
	int r = -1;
	int base = 10;
	if( ! (*txtpos >= '0' && *txtpos <= '9') ) {
		error = 1;
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
	error = errno;
	if( !error )
		txtpos = next;
	return r;
}

static float parse_float() {
	float r;
	if( *txtpos < '0' && *txtpos > '9' && *txtpos != '-' ) {
		error = 1;
		return r;
	} 
	errno = 0;
	r = strtof(txtpos,&next);
	error = errno;
	if( !error )
		txtpos = next;
	return r;
}

#define FUNC_T       0
#define FUNC_MS      1
#define FUNC_PI      2
#define FUNC_IF      3
#define FUNC_ABS     4
#define FUNC_INT     5
#define FUNC_FLOAT   6
#define FUNC_SIN     7
#define FUNC_POW     8
#define FUNC_CEIL    9
#define FUNC_FLOOR  10
#define FUNC_RAND   11
#define FUNC_ROUND  12
#define FUNC_SERIES 13
const char func_table[] = {
  't'|0x80,
  'm','s'|0x80,
  'p','i'|0x80,
  'i','f'|0x80,
  'a','b','s'|0x80,
  'i','n','t'|0x80,
  'f','l','o','a','t'|0x80,
  's','i','n'|0x80,
  'p','o','w'|0x80,
  'c','e','i','l'|0x80,
  'f','l','o','o','r'|0x80,
  'r','a','n','d'|0x80,
  'r','o','u','n','d'|0x80,
  's','e','r','i','e','s'|0x80,
  0x00
};

#define PNTTYPE_DO 0
#define PNTTYPE_DI 1
#define PNTTYPE_AO 2
#define PNTTYPE_AI 3
const char pnttype_table[] = {
  'd','o'|0x80,
  'd','i'|0x80,
  'a','o'|0x80,
  'a','i'|0x80,
  0x00
};

const char scaled_table[] = {
  's','c','a','l','e','d'|0x80,
  0x00
};

#define CMD_NEW    0
#define CMD_LIST   1
#define CMD_STATE  2
#define CMD_UNDEF  3
#define CMD_MODBUS 4
#define CMD_STEP   5
#define CMD_LED    6
const char cmd_table[]  = {
  'n','e','w'|0x80,
  'l','i','s','t'|0x80,
  's','t','a','t','e'|0x80,
  'u','n','d','e','f'|0x80,
  'm','o','d','b','u','s'|0x80,
  's','t','e','p'|0x80,
  'l','e','d'|0x80,
  0x00
};

int scan_table(const char* table,char* str, unsigned int len) {
  int idx = 0;
  const char* ttable = table;
  char* tstr;
  while( *ttable != 0 ) {
     tstr = str;
     while( ((*ttable)&0x7F) == *tstr || ((*ttable)&0x7F) == (*tstr+32)) {
    if( ((*ttable)&0x80) && ((unsigned int)(tstr-str+1) == len) ) {
        return idx;
      }
      ttable++;
      tstr++;
     }
     while( ((*ttable)&0x80) == 0 )
      ttable++;
     ttable++;
     idx++;
  }
  return -1;
}

#define FUNC_START() txtpos=next; ignore_blanks(); if( *txtpos != '(' ) { error = 1; return a; } txtpos++;
#define FUNC_FIRST_ARG(v) FUNC_START(); ignore_blanks(); v=expr1(); if( error ) { return a; }
#define FUNC_NEXT_ARG(v) ignore_blanks(); if( *txtpos != ',' ) { error = 1; return a; } txtpos++; v=expr1(); if( error ) { return a; }
#define FUNC_END() ignore_blanks(); if( *txtpos != ')' ) { error = 1; return a; } txtpos++;
#define FUNC_CHECK_COMMA() ignore_blanks(); if( *txtpos != ',' ) { error=1; return a; } txtpos++;

static void func_skip_arg() {
	unsigned int level = 1;
	txtpos++;
	while( 1 ) {
		if( *txtpos == '\n' || *txtpos == 0 ) {
			error = 1;
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
				error = 1;
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
		if( *tmp == '\n' || *tmp == 0 ) {
			error = 1;
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
				error = 1;
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
	a.type = VAL_INT;
	a.i = 0;
	
	ignore_blanks();
	
	//Handle Negations
	if( *txtpos == '-' ) {
		txtpos++;
		a = expr7();
		if( error ) return a;
		if( a.type == VAL_INT )
			a.i = -a.i;
		else
			a.f = -a.f;
		return a;
	}
	
	//Handle Logical Negations
	if( *txtpos == '!' ) {
		txtpos++;
		a = expr7();
		if( error ) return a;
		if( IS_VAL(a) ) {
			a.type = VAL_INT;
			a.i = 0;
		}
		else {
			a.type = VAL_INT;
			a.i = 1;
		}
	}
	
	//Handle postive constants
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
			a.type = VAL_FLOAT;
			a.f = 0;
			if( base != 10 )
				error = 1;
			if( !error ) {
				errno = 0;
				a.f = strtof(txtpos,&next);
				error = errno;
			}
			if( !error )
				txtpos = next;
		}
		else {
			//Constant is an integer
			errno = 0;
			a.type = VAL_INT;
			a.i = (int)strtol(txtpos,&next,base);
			error = errno;
			if( !error )
				txtpos = next;
		}
		return a;
	}
	
	if(*txtpos == '(') {
		txtpos++;
		a = expr1();
		if( *txtpos != ')' ) {
			error = 1;
		}
		txtpos++;
		return a;
	}
	
	ignore_blanks();
	parse_name();
	
	if( next-txtpos == 0 ) {
		error = 1;
		a.type = VAL_INT;
		a.i = 0;
		return a;
	}
  
  idx = scan_table(func_table,txtpos,next-txtpos);
  if( idx >= 0 ) {
    val_t b;
    switch(idx) {
      case FUNC_T:
        txtpos = next;
        a.type = VAL_INT;
        a.i = ticks;
        break;
      case FUNC_PI:
        txtpos = next;
        a.type = VAL_FLOAT;
        a.f = M_PI;
        break;
  #ifdef ARDUINO
      case FUNC_MS:
        txtpos = next;
        a.type = VAL_INT;
        a.i = millis();
        break;
  #endif //ARDUINO
      case FUNC_IF:
        FUNC_FIRST_ARG(a);
        if( IS_VAL(a) ) {
          FUNC_NEXT_ARG(a);
          ignore_blanks();
          if( *txtpos == ',' ) {
            txtpos++;
            func_skip_arg();
          }
          if( ! error ) {
            FUNC_END();
          }
        }
        else {
          ignore_blanks();
          if( *txtpos != ',' ) {
            error = 1;
            return a;
          }
          func_skip_arg();
          if( ! error ) {
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
        if( a.type == VAL_INT ) a.i = abs(a.i);
        else a.f = fabs(a.f);
        break;
      case FUNC_INT:
        FUNC_FIRST_ARG(a);
        FUNC_END();
        MAKE_INT(a);
        break;
      case FUNC_SIN:
            FUNC_FIRST_ARG(a);
        FUNC_END();
        MAKE_FLOAT(a);
        a.f = sin(a.f);
        break;
      case FUNC_POW:
        FUNC_FIRST_ARG(a);
        FUNC_NEXT_ARG(b);
        FUNC_END();
        MAKE_FLOAT(a);
        MAKE_FLOAT(b);
        a.f = pow(a.f,b.f);
        break;
      case FUNC_CEIL:
        FUNC_FIRST_ARG(a);
        FUNC_END();
        if( a.type == VAL_FLOAT )
          a.f = ceil(a.f);
        break;
      case FUNC_RAND:
        FUNC_FIRST_ARG(a);
        FUNC_NEXT_ARG(b);
        FUNC_END();
        MAKE_INT(a);
        MAKE_INT(b);
#ifdef ARDUINO
        a.i = random(a.i,b.i);
#else
        a.i = a.i + random()%(b.i-a.i);
#endif //ARDUINO
        break;
      case FUNC_FLOAT:
        FUNC_FIRST_ARG(a);
        FUNC_END();
        MAKE_FLOAT(a);
        break;
      case FUNC_FLOOR:
        FUNC_FIRST_ARG(a);
        FUNC_END();
        if( a.type == VAL_FLOAT )
          a.f = floor(a.f);
        break;
      case FUNC_ROUND:
        FUNC_FIRST_ARG(a);
        FUNC_END();
        if( a.type == VAL_FLOAT )
          a.f = round(a.f);
        break;
      case FUNC_SERIES:
        {
          unsigned int arg_count;
          unsigned int arg_idx;
          FUNC_START();
          arg_count = func_arg_count();
          if( error ) {
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
      default:
        error = 1;    
    }
    return a;
  }

	//Assume that this is a variable
	//get_var will create it if is does not already exist
	var_t *v = get_var(txtpos,next-txtpos);
	if( !v ) {
		error = 1;
		return a;
	}
	a = v->value;
	txtpos = next;
	return a;
}

//Precedence: * / %
static val_t expr6() {
	val_t a;
	
	a = expr7();
	if( error ) return a;
	
	ignore_blanks();
	while( 1 ) {
		if( *txtpos == '*' ) {
			val_t b;
			txtpos++;
			b = expr7();
			if( error ) return a;
			if( a.type == VAL_FLOAT || b.type == VAL_FLOAT ) {
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
			txtpos++;
			b = expr7();
			if( error ) return a;
			if( a.type == VAL_FLOAT || b.type == VAL_FLOAT ) {
				MAKE_FLOAT(a);
				MAKE_FLOAT(b);
				if( b.f == 0 ) {
					error = 1;
					return a;
				}
				a.f = a.f / b.f;
			}
			else {
				if( b.i == 0 ) {
					error = 1;
					return a;
				}
				a.i = a.i / b.i;
			}
		}
		else if( *txtpos == '%' ) {
			val_t b;
			txtpos++;
			b = expr7();
			if( error ) return a;
			MAKE_INT(a);
			MAKE_INT(b);
			if( b.i == 0 ) {
				error = 1;
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
	if( error ) return a;
	
	ignore_blanks();
	while( 1 ) {
		if( *txtpos == '+' ) {
			val_t b;
			txtpos++;
			b = expr6();
			if( error ) return a;
			if( a.type == VAL_FLOAT || b.type == VAL_FLOAT ) {
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
			if( error ) return a;
			if( a.type == VAL_FLOAT || b.type == VAL_FLOAT ) {
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
	if( error ) return a;
	
	ignore_blanks();
	while( 1 ) {
		if( *txtpos == '<' && *(txtpos+1) == '<' ) {
			val_t b;
			txtpos = txtpos + 2;
			b = expr5();
			if( error ) return a;
			MAKE_INT(a);
			MAKE_INT(b);
			a.i = a.i << b.i;
		}
		if( *txtpos == '>' && *(txtpos+1) == '>' ) {
			val_t b;
			txtpos = txtpos + 2;
			b = expr5();
			if( error ) return a;
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
	if( error ) return a;
	
	ignore_blanks();
	while( 1 ) {
		if( *txtpos == '=' && *(txtpos+1) == '=' ) {
			val_t b;
			txtpos = txtpos + 2;
			b = expr4();
			if( error ) return a;
			
			if( a.type == VAL_FLOAT || b.type == VAL_FLOAT ) {
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
			if( error ) return a;
			
			if( a.type == VAL_FLOAT || b.type == VAL_FLOAT ) {
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
      if( error ) return a;
      
      if( a.type == VAL_FLOAT || b.type == VAL_FLOAT ) {
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
      if( error ) return a;
      
      if( a.type == VAL_FLOAT || b.type == VAL_FLOAT ) {
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
      if( error ) return a;
      
      if( a.type == VAL_FLOAT || b.type == VAL_FLOAT ) {
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
      if( error ) return a;
      
      if( a.type == VAL_FLOAT || b.type == VAL_FLOAT ) {
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
	if( error ) return a;
	
	ignore_blanks();
	while( 1 ) {
		if( *txtpos == '|' && *(txtpos+1) != '|' ) {
			val_t b;
			txtpos++;
			b = expr3();
			if( error ) return a;
			MAKE_INT(a);
			MAKE_INT(b);
			a.i = a.i | b.i;
		}
    else if( *txtpos == '^' ) {
      val_t b;
      txtpos++;
      b = expr3();
      if( error ) return a;
      MAKE_INT(a);
      MAKE_INT(b);
      a.i = a.i ^ b.i;
    }
    else if( *txtpos == '&' && *(txtpos+1) != '&' ) {
      val_t b;
      txtpos++;
      b = expr3();
      if( error ) return a;
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
	if( error ) return a;
	
	ignore_blanks();
	while( 1 ) {
		if( *txtpos == '|' && *(txtpos+1) == '|' ) {
			val_t b;
			txtpos = txtpos + 2;
			b = expr2();
			if( error ) return a;
			
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
      if( error ) return a;;
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

static int parse_expression(val_t *a) {
	if( *txtpos != '?' ) {
		return 0;
	}
	txtpos++;
	ignore_blanks();
	*a = expr1();
	if( error  )
		return 0;
	ignore_blanks();
	if( *txtpos != '\n' && *txtpos != 0 )
		return 0;
	return 1;
}

static var_t* parse_assignment(var_t* var, char* name, unsigned int namelen) {
	char* expr;
	val_t a;
	
	if( var == 0 && (name==0 || namelen == 0) ) {
		return 0;
	}
	
	if( *txtpos != '=' ) {
		return 0;
	}
	txtpos++;
	ignore_blanks();
	
	expr = txtpos;
	a = expr1();
	if( error )
		return 0;
	ignore_blanks();
	
	if( *txtpos != ':' && *txtpos != '\n' && *txtpos != 0 )
		return 0;
	
	if( ! var ) {
		var = get_var(name,namelen);
		if( ! var )
			return 0;
	}
	if( var->expr != 0 ) {
		del_expr(var->expr);
	}
	var->expr = add_expr(expr);
	if( ! var->expr ) {
		del_var(var);
		return 0;
	}
	var->value = a;
	return var;
}

static var_t* parse_point(var_t *var, char* name, unsigned int namelen) {
	uint8_t pnttype = 0;
	unsigned int pntaddr = 0;
	float pntmin = 0;
	float pntmax = 0;
  int table_idx;
	if( var == 0 && (name==0 || namelen == 0) ) {
		return 0;
	}
	
	if( *txtpos != ':' ) {
		return 0;
	}
	txtpos++;
	ignore_blanks();
	
	parse_name();
	if( next-txtpos != 2 ) {
		return 0;
	}

  table_idx = scan_table(pnttype_table,txtpos,2);
  switch( table_idx ) {
    case PNTTYPE_DO:
      pnttype = PNT_DO;
      txtpos = next;
      ignore_blanks();
      pntaddr = parse_unsigned_int();
      break;
    case PNTTYPE_DI:
      pnttype = PNT_DI;
      txtpos = next;
      ignore_blanks();
      pntaddr = parse_unsigned_int();
      break;
    case PNTTYPE_AO:
      txtpos = next;
      ignore_blanks();
      parse_name();
      if( scan_table(scaled_table,txtpos,next-txtpos) == 0 ) {
        pnttype = PNT_AO_SCALED;
        ignore_blanks();
        pntaddr = parse_unsigned_int();
        if( ! error )
          pntmin = parse_float();
        if( ! error )
          pntmax = parse_float();
      }
      else {
        pnttype = PNT_AO;
        pntaddr = parse_unsigned_int();
      }
      break;
    case PNTTYPE_AI:
      txtpos = next;
      ignore_blanks();
      parse_name();
      if( scan_table(scaled_table,txtpos,next-txtpos) == 0 ) {
        pnttype = PNT_AI_SCALED;
        ignore_blanks();
        pntaddr = parse_unsigned_int();
        if( ! error )
          pntmin = parse_float();
        if( ! error )
          pntmax = parse_float();
      }
      else {
        pnttype = PNT_AI;
        pntaddr = parse_unsigned_int();
      }
      break;
    default:
      error = 1;
  }
	
	if( error )
		return 0;

	if( ! var ) {
		var = get_var(name,namelen);
		if( ! var )
			return 0;
	}
	var->pnttype = pnttype;
	var->pntaddr = pntaddr;
	var->pntmin = pntmin;
	var->pntmax = pntmax;
	return var;
}

void expr_tick();
static int parse_command(char* cmd, unsigned cmdlen) {
  int table_idx;
  table_idx = scan_table(cmd_table,cmd,cmdlen);
  if( table_idx < 0 ) {
    return 0;
  }
  switch( table_idx ) {
    case CMD_NEW:
      var_init();
#ifdef MINI
      indicatorsReset();
#endif
      break;
    case CMD_LIST:
      cli_print_list();
      break;
    case CMD_STATE:
      cli_print_state();    
      break;
    case CMD_UNDEF:
      txtpos = next;
      ignore_blanks();
      parse_name();
      if( next != txtpos )
        del_var(get_var(txtpos,next-txtpos));    
      break;
#ifdef ARDUINO
    case CMD_MODBUS:
      modbus_address = (uint8_t)parse_unsigned_int();
      break;
#endif
#ifndef ARDUINO
    case CMD_STEP:
      expr_tick();
      break;
#endif
#ifdef MINI
    case CMD_LED:
      {
        unsigned int led_idx;
        int pnttable_idx;
        unsigned int pntaddr = 0;
        unsigned int cutoff = 0;
        char* pnttype = 0;
        ignore_blanks();
        led_idx = parse_unsigned_int();
        if( ! error ) {
          ignore_blanks();
          parse_name();
          pnttype = txtpos;
          if( next-txtpos != 2 )
            error = 1;
          else
            txtpos = next;
        }
        if( ! error ) {
          pnttable_idx = scan_table(pnttype_table,pnttype,2);
          if( pnttable_idx < 0 )
            error = 1;
          else {
            ignore_blanks();
            pntaddr = parse_unsigned_int();
            switch( pnttable_idx ) {
              case PNTTYPE_DO:
                error = ! indicatorsConfig(led_idx,IND_DO,pntaddr,0);
                break;
              case PNTTYPE_DI:
                error = ! indicatorsConfig(led_idx,IND_DI,pntaddr,0);
                break;
              case PNTTYPE_AO:
                ignore_blanks();
                cutoff = parse_unsigned_int();
                if( ! error )
                  error = ! indicatorsConfig(led_idx,IND_AO,pntaddr,cutoff);
                break;
              case PNTTYPE_AI:
                ignore_blanks();
                cutoff = parse_unsigned_int();
                if( ! error )
                  error = ! indicatorsConfig(led_idx,IND_AI,pntaddr,cutoff);
                break;
              default:
                error = 1;
            }
          }
        }
        break;
      }
#endif //MINI
    default:
      return 0;
  }
  return 1;
}

int expr_interp(char* line) {
	var_t *var;
	char* name;
	unsigned int namelen;

  txtpos = line;
  error = 0;
  
	ignore_blanks();
	
	//Parse Simple expression evaluations
	if( *txtpos == '?' ) {
		val_t a;
		if( ! parse_expression(&a) )
			return (txtpos-line+1);
		cli_print_val(a);
		return 0;
	}
	
	//Parse the variable name being configured
	parse_name();
	if( next == txtpos )
		return (txtpos-line+1);
	name = txtpos;
	namelen = next-txtpos;
	txtpos = next;
	ignore_blanks();
	
	if( parse_command(name,namelen) ) {
    if( error )
      return (txtpos-line+1);
    else
      return 0;
	}
	
	//Parse assigment and then (possibily) modbus configuration
	if( *txtpos == '=' ) {
		var = parse_assignment(0,name,namelen);
		if( ! var ) {
			return (txtpos-line+1);
		}
		ignore_blanks();
		if( *txtpos == ':' ) {
			var = parse_point(var,0,0);
			if( !var ) {
				return (txtpos-line+1);
			}
		}
	}
	//Parse modbus configuration and then (possibily) assignment
	else if( *txtpos == ':' ) {
		var = parse_point(0,name,namelen);
		if( ! var ) {
			return (txtpos-line+1);
		}
		ignore_blanks();
		if( *txtpos == '=' ) {
			var = parse_assignment(var,0,0);
			if( ! var ) {
				return (txtpos-line+1);
			}
		}
	}
	else {
		return (txtpos-line+1);
	}
	
	return 0;
}

void expr_tick() {
	unsigned int idx;
	ticks++;
	for( idx=0; idx<VARSMAX; idx++ ) {
		if( vars[idx].expr != 0 ) {
			txtpos = vars[idx].expr;
      error = 0;
			vars[idx].value = expr1();
			if( error ) {
				cli_print_eval_error(idx,txtpos-vars[idx].expr+1);
			}
		}
	}
}
