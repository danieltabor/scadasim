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
#define __DISPLAY_C__
#include "display.h"
#include "var.h"
#include "compat.h"
#include "cli.h"

#include <stdio.h>
#include <string.h>

static char gfxdata[GFXSIZE+1];
char gfxpath[256];
static char gfxval[10];
static unsigned int last_display_millis;
static uint8_t gfxrun;


void displayBegin() {
	gfxdata[0] = 0;
	gfxpath[0] = 0;
	gfxrun = 0;
}

void displayRun() {
	if( gfxdata[0] )
		gfxrun = 1;
}

void displayStop() {
	gfxrun = 0;
}

void displayLoad(char* path) {
	FILE* fp;
	unsigned int len = 0;
	char* c;
	if( path ) {
		c = path;
		while( *c && len<255 ) {
			gfxpath[len++] = *c;
			c++;
		}
		gfxpath[len] = 0;
		if( len ) {
			last_display_millis = 0;
			fp = fopen(path,"rb");
			if( fp ) {
				len = fread(gfxdata,1,GFXSIZE,fp);
				fclose(fp);
				if( len == GFXSIZE ) {
					len = 0;
				}
			}
		}
	}
	if( len == 0 ) {
		gfxpath[0] = 0;
	}
	gfxdata[len] = 0;
	gfxrun = 0;
}

void displayProcess() {
	unsigned int millis;
	var_t* v;
	char* n;
	char* p;
	if( gfxrun ) {
		millis = compatMillis();
		if( millis - last_display_millis > DISPLAYDELAY ) {
			last_display_millis = millis;
			displayOut(0x1b);
			displayOut('[');
			displayOut('2');
			displayOut('J');
			n = gfxdata;
			while( *n != 0 ) {
				displayOut(*n);
				if( *n == ':' ) {
					p = n-1;
					while( (p > gfxdata) && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9')) )
						p--;
					p++;
					while( (*p >= '0' && *p <= '9') )
						p++;
					while( 1 ) {
						v = get_var(p,n-p);
						if( v == 0 && ((*p >= 'A' && *p <= 'H') || (*p == 'J') || (*p == 'K') || (*p == 'S') || (*p == 'T') || (*p == 'f') || (*p == 'm') || (*p == 'i') || (*p == 'n')) ) {
							p++;
						} else {
							break;
						}
					}
					n++;
					if( v ) {
						if( v->value.type == VAL_INT )
							snprintf(gfxval,10,"%d",v->value.i);
						else if( v->value.type == VAL_FLOAT )
							snprintf(gfxval,10,"%0.3f",v->value.f);
						else
							snprintf(gfxval,10,"?");
						p = gfxval;
						while( *p ) {
							displayOut(*p);
							p++;
							if( *n == ' ' ) {
								n++;
							}
						}
					}
				} else {
					n++;
				}
			}
		}
	}
}
