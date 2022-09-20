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
#include "pointvar.h"
#include "var.h"

int setDO(uint16_t do_addr, uint8_t value) {
	unsigned int i;
	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].pnttype == PNT_DO && vars[i].pntaddr == do_addr ) {
			vars[i].value.type == VAL_INT;
			vars[i].value.i = value;
			if( vars[i].expr ) {
				set_expr(vars+i,0,0);
			}
			return 1;
		}
	}
	return 0;
}

int getDO(uint16_t do_addr, uint8_t *value) {
	unsigned int i;
	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].pnttype == PNT_DO && vars[i].pntaddr == do_addr ) {
			if( (vars[i].value.type == VAL_INT && vars[i].value.i != 0 ) ||
				(vars[i].value.type == VAL_FLOAT && vars[i].value.f != 0.0 ) ) {
					*value = 1;
			}
			else {
				*value = 0;
			}
			return 1;
		}
	}
	return 0;
}

int getDI(uint16_t di_addr, uint8_t *value) {
	unsigned int i;
	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].pnttype == PNT_DI && vars[i].pntaddr == di_addr ) {
			if( (vars[i].value.type == VAL_INT && vars[i].value.i != 0 ) ||
				(vars[i].value.type == VAL_FLOAT && vars[i].value.f != 0.0 ) ) {
					*value = 1;
			}
			else {
				*value = 0;
			}
			return 1;
		}
	}
	return 0;
}

int getAI(uint16_t ai_addr, uint16_t *value) {
	unsigned int i;
	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].pntaddr == ai_addr ) {
			if( vars[i].pnttype == PNT_AI ) {
				if( vars[i].value.type == VAL_INT ) {
					*value = (uint16_t)vars[i].value.i;
				}
				else {
					*value = (uint16_t)vars[i].value.f;
				}
				return 1;
			}
			else if( vars[i].pnttype == PNT_AI_SCALED ) {
				float v;
				if( vars[i].value.type == VAL_INT ) {
					if( (float)vars[i].value.i <= vars[i].pntmin ) { 
						*value = 0;
						return 1;
					}
					else
						v = (float)vars[i].value.i - vars[i].pntmin;
				}
				else {
					v = vars[i].value.f - vars[i].pntmin;
				}
				*value = v/(vars[i].pntmax-vars[i].pntmin)*0xFFFF;
				return 1;
			}
		}
	}
	return 0;
}

int setAO(uint16_t ao_addr, uint16_t value) {
	unsigned int i;
	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].pntaddr == ao_addr ) {
			if( vars[i].pnttype == PNT_AO ) {
				vars[i].value.type = VAL_INT;
				vars[i].value.i = value;
				set_expr(vars+i,0,0);
				return 1;
			}
			else if( vars[i].pnttype == PNT_AO_SCALED ) {
				float v = (float)value * (vars[i].pntmax - vars[i].pntmin);
				vars[i].value.type = VAL_FLOAT;
				if( v == 0 ) {
					vars[i].value.f = 0.0;
				}
				else {
					vars[i].value.f = v / (float)0xFFFF;
				}
				set_expr(vars+i,0,0);
				return 1;
			}
		}
	}
	return 0;
}

int getAO(uint16_t ao_addr, uint16_t *value) {
	unsigned int i;
	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].pntaddr == ao_addr ) {
			if( vars[i].pnttype == PNT_AO ) {
				if( vars[i].value.type == VAL_INT ) {
					*value = (uint16_t)vars[i].value.i;
				}
				else {
					*value = (uint16_t)vars[i].value.f;
				}
				return 1;
			}
			else if( vars[i].pnttype == PNT_AO_SCALED ) {
				float v;
				if( vars[i].value.type == VAL_INT ) {
					if( (float)vars[i].value.i <= vars[i].pntmin ) { 
						*value = 0;
						return 1;
					}
					else
						v = (float)vars[i].value.i - vars[i].pntmin;
				}
				else {
					v = vars[i].value.f - vars[i].pntmin;
				}
				*value = (uint16_t) (v*(float)0xFFFF/(vars[i].pntmax-vars[i].pntmin));
				return 1;
			}
		}
	}
	return 0;
}

