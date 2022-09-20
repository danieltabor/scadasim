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
#include <stdlib.h>

#include "command.h"

#ifdef MINI
#include "indicators.h"
#endif

#include "var.h"
#include "parse.h"
#include "expr.h"
#include "cli.h"
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
#define CMD_UNDEF  2
#define CMD_MODBUS 3
#define CMD_STATE  4

#ifndef ARDUINO
#define CMD_EXIT   5
#define CMD_SAVE   6
#define CMD_LOAD   7
#define CMD_GFX    8
#define CMD_RUN    9
#define CMD_STOP  10
#define CMD_MODBUSTCP 11
#define CMD_IEC61850 12
#define CMD_GOOSE_DIGITAL 13
#define CMD_GOOSE_ANALOG  14
#define CMD_ICD       15
#define CMD_SCD       16
#endif //not ARDUINO

#ifdef MINI
#define CMD_LED    5
#endif //MINI

const char cmd_table[]	= {
	'n','e','w'|0x80,
	'l','i','s','t'|0x80,
	'u','n','d','e','f'|0x80,
	'm','o','d','b','u','s'|0x80,
	's','t','a','t','e'|0x80,
#ifndef ARDUINO
	'e','x','i','t'|0x80,
	's','a','v','e'|0x80,
	'l','o','a','d'|0x80,
	'g','f','x'|0x80,
	'r','u','n'|0x80,
	's','t','o','p'|0x80,
	'm','o','d','b','u','s','t','c','p'|0x80,
	'i','e','c','6','1','8','5','0'|0x80,
	'g','s','e','d'|0x80,
	'g','s','e','a'|0x80,
	'i','c','d'|0x80,
	's','c','d'|0x80,
#endif //not ARDUINO
#ifdef MINI
	'l','e','d'|0x80,
#endif //MINI
	0x00
};


static var_t* parse_assignment(var_t* var) {
	char* expr;
	val_t a;
	
	//*txtpos == '='
	txtpos++;
	ignore_blanks();
	
	expr = txtpos;
	if( ! expr_eval(&a) )
		return 0;
	ignore_blanks();
	
	if( ! set_expr(var,expr,txtpos-expr) ) {
		return 0;
	}
	
	var->value = a;
	return var;
}

static var_t* parse_pointvar(var_t *var) {
	uint8_t pnttype = 0;
	unsigned int pntaddr = 0;
	float pntmin = 0;
	float pntmax = 0;
	int table_idx;
	
	//*txtpos == ':'
	txtpos++;
	ignore_blanks();
	
	parse_name();
	if( next-txtpos != 2 ) {
		return 0;
	}

	table_idx = table_scan(pnttype_table,txtpos,2);
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
		if( table_scan(scaled_table,txtpos,next-txtpos) == 0 ) {
		pnttype = PNT_AO_SCALED;
		txtpos = next;
		ignore_blanks();
		pntaddr = parse_unsigned_int();
		if( ! parse_error )
			pntmin = parse_float();
		if( ! parse_error )
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
		if( table_scan(scaled_table,txtpos,next-txtpos) == 0 ) {
		pnttype = PNT_AI_SCALED;
		txtpos = next;
		ignore_blanks();
		pntaddr = parse_unsigned_int();
		if( ! parse_error )
			pntmin = parse_float();
		if( ! parse_error )
			pntmax = parse_float();
		}
		else {
		pnttype = PNT_AI;
		pntaddr = parse_unsigned_int();
		}
		break;
	default:
		parse_error = 1;
	}
	
	if( parse_error )
		return 0;

	var->pnttype = pnttype;
	var->pntaddr = pntaddr;
	var->pntmin = pntmin;
	var->pntmax = pntmax;
	return var;
}

static int parse_command(char* cmd, unsigned cmdlen) {
	int table_idx;
	val_t val;
	table_idx = table_scan(cmd_table,cmd,cmdlen);
	if( table_idx < 0 ) {
		return 0;
	}
	switch( table_idx ) {
		case CMD_NEW:
			commandBegin();
			break;
		case CMD_LIST:
			cli_print_list();
			break;
		case CMD_UNDEF:
			txtpos = next;
			ignore_blanks();
			parse_name();
			if( next != txtpos )
			del_var(get_var(txtpos,next-txtpos));
			break;
		case CMD_MODBUS:
			#ifdef MODBUS
				modbus_address = (uint8_t)parse_unsigned_int();
			#else
				while( *txtpos != 0 ) { txtpos++; }
			#endif
			break;
		case CMD_STATE:
			cli_print_state();
			break;
#ifndef ARDUINO
		case CMD_EXIT:
			#ifdef IEC61850
				iec61850Reset();
			#endif
			compatExit();
			break;
		case CMD_SAVE:
			ignore_blanks();
			cli_start_save(txtpos);
			break;
		case CMD_LOAD:
			commandBegin();
			ignore_blanks();
			cli_start_load(txtpos);
			break;
		case CMD_GFX:
			ignore_blanks();
			displayLoad(txtpos);
			if( gfxpath[0] == 0 ) {
				parse_error = 1;
			}
			break;
		case CMD_RUN:
			displayRun();
			break;
		case CMD_STOP:
			displayStop();
			break;
		case CMD_MODBUSTCP:
			#ifdef MODBUSTCP
			{
				char* portpos = txtpos;
				modbustcp_port = (uint16_t)parse_unsigned_int();
				if( modbusTcpServ() ) {
					txtpos = portpos;
					parse_error = 1;
					break;
				}
			}
			#else
				while( *txtpos != 0 ) { txtpos++; }
			#endif
			break;
		case CMD_IEC61850:
			#ifdef IEC61850
			{
				int i;
				iec61850ServReset();
				txtpos = next;
				ignore_blanks();
				parse_name();
				if( txtpos == next ) { parse_error=1; break; }
				for( i=0; txtpos != next && i < 255; i++, txtpos++ ) {
					iec61850_serv_name[i] = *txtpos;
				}
				iec61850_serv_name[i] = 0;
				
				ignore_blanks();
				iec61850_serv_port = (uint16_t)parse_unsigned_int();
				if( parse_error ) { iec61850ServReset(); break; }
				
				iec61850Serv();
			}
			#else
				while( *txtpos != 0 ) { txtpos++; }
			#endif //IEC61850
			break;
		case CMD_GOOSE_DIGITAL:
			#ifdef IEC61850
			{
				int i, j;
				unsigned char b;
				char* ethpos;
				if( ! iec61850_serv_name[0] ) {
					parse_error = 1;
					break;
				}
				
				iec61850GooseDigitalReset();
				txtpos = next;
				
				ignore_blanks();
				parse_name();
				if( txtpos == next ) { parse_error=1; break; }
				ethpos = txtpos;
				for( i=0; txtpos != next && i < 255; i++, txtpos++ ) {
					iec61850_goose_digital_eth[i] = *txtpos;
				}
				iec61850_goose_digital_eth[i] = 0;
				txtpos = next;
				
				ignore_blanks();
				iec61850_goose_digital_appid = (uint16_t)parse_unsigned_int();
				if( parse_error ) { break; }
				
				ignore_blanks();
				for( i=0; i<6; i++ ) {
					for( b=0, j=0; j<2; txtpos++, j++ ) {
						b = b << 4;
						if( *txtpos >= 'A' && *txtpos <= 'F' ) {
							b =  b | (*txtpos-'A'+10);
						}
						else if( *txtpos >= 'a' && *txtpos <= 'f' ) {
							b = b | (*txtpos-'a'+10);
						}
						else if( *txtpos >= '0' && *txtpos <= '9' ) {
							b = b | (*txtpos-'0');
						}
						else {
							parse_error = 1;
							break;
						}
					}
					if( parse_error ) { break; }
					if( i < 5 ) {
						if( *txtpos != ':' ) {
							parse_error = 1;
							break;
						} else {
							txtpos++;
						}
					}
					iec61850_goose_digital_dst[i] = b;
				}
				if( parse_error ) { iec61850GooseDigitalReset(); break; }
				
				iec61850GooseDigital();
				if( ! iec61850_goose_digital_eth[0] ) {
					txtpos = ethpos;
					parse_error = 1;
					break;
				}
			}
			#else
				while( *txtpos != 0 ) { txtpos++; }
			#endif //IEC61850
			break;
		case CMD_GOOSE_ANALOG:
			#ifdef IEC61850
			{
				int i, j;
				unsigned char b;
				char* ethpos;
				if( ! iec61850_serv_name[0] ) {
					parse_error = 1;
					break;
				}
				
				iec61850GooseAnalogReset();
				txtpos = next;
				
				ignore_blanks();
				parse_name();
				if( txtpos == next ) { parse_error=1; break; }
				ethpos = txtpos;
				for( i=0; txtpos != next && i < 255; i++, txtpos++ ) {
					iec61850_goose_analog_eth[i] = *txtpos;
				}
				iec61850_goose_analog_eth[i] = 0;
				txtpos = next;
				
				ignore_blanks();
				iec61850_goose_analog_appid = (uint16_t)parse_unsigned_int();
				if( parse_error ) { break; }
				
				ignore_blanks();
				for( i=0; i<6; i++ ) {
					for( b=0, j=0; j<2; txtpos++, j++ ) {
						b = b << 4;
						if( *txtpos >= 'A' && *txtpos <= 'F' ) {
							b =  b | (*txtpos-'A'+10);
						}
						else if( *txtpos >= 'a' && *txtpos <= 'f' ) {
							b = b | (*txtpos-'a'+10);
						}
						else if( *txtpos >= '0' && *txtpos <= '9' ) {
							b = b | (*txtpos-'0');
						}
						else {
							parse_error = 1;
							break;
						}
					}
					if( parse_error ) { break; }
					if( i < 5 ) {
						if( *txtpos != ':' ) {
							parse_error = 1;
							break;
						} else {
							txtpos++;
						}
					}
					iec61850_goose_analog_dst[i] = b;
				}
				if( parse_error ) { iec61850GooseAnalogReset(); break; }
				
				iec61850GooseAnalog();
				if( ! iec61850_goose_analog_eth[0] ) {
					txtpos = ethpos;
					parse_error = 1;
					break;
				}
			}
			#else
				while( *txtpos != 0 ) { txtpos++; }
			#endif //IEC61850
			break;
		case CMD_ICD:
			#ifdef IEC61850
			{
				int i;
				txtpos = next;
				ignore_blanks();
				i=0;
				while( i < 255 && *txtpos != ' ' && *txtpos != '\t' && *txtpos != '\n' && *txtpos != '\r' ) {
					iec61850_icd_path[i++] = *txtpos++;
				}
				iec61850_icd_path[i] = 0;
				
				iec61850ExportIcd();
			}
			#else
				while( *txtpos != 0 ) { txtpos++; }
			#endif //IEC61850
			break;
		case CMD_SCD:
			#ifdef IEC61850
			{
				int i;
				txtpos = next;
				ignore_blanks();
				i=0;
				while( i < 255 && *txtpos != ' ' && *txtpos != '\t' && *txtpos != '\n' && *txtpos != '\r' ) {
					iec61850_scd_path[i++] = *txtpos++;
				}
				iec61850_scd_path[i] = 0;
				
				iec61850ExportScd();
			}
			#else
				while( *txtpos != 0 ) { txtpos++; }
			#endif //IEC61850
			break;
#endif //not ARDUINO
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
			if( ! parse_error ) {
				ignore_blanks();
				parse_name();
				pnttype = txtpos;
				if( next-txtpos != 2 )
				parse_error = 1;
				else
				txtpos = next;
			}
			if( ! parse_error ) {
				pnttable_idx = table_scan(pnttype_table,pnttype,2);
				if( pnttable_idx < 0 )
				parse_error = 1;
				else {
					ignore_blanks();
					pntaddr = parse_unsigned_int();
					switch( pnttable_idx ) {
						case PNTTYPE_DO:
							parse_error = ! indicatorsConfig(led_idx,IND_DO,pntaddr,0);
							break;
						case PNTTYPE_DI:
							parse_error = ! indicatorsConfig(led_idx,IND_DI,pntaddr,0);
							break;
						case PNTTYPE_AO:
							ignore_blanks();
							cutoff = parse_unsigned_int();
							if( ! parse_error )
								parse_error = ! indicatorsConfig(led_idx,IND_AO,pntaddr,cutoff);
							break;
						case PNTTYPE_AI:
							ignore_blanks();
							cutoff = parse_unsigned_int();
							if( ! parse_error )
								parse_error = ! indicatorsConfig(led_idx,IND_AI,pntaddr,cutoff);
							break;
						default:
							parse_error = 1;
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

void commandBegin() {
	varBegin();
	#ifdef MINI
			indicatorsReset();
	#endif
	#ifdef MODBUS
			modbusBegin();
	#endif
	#ifdef MODBUSTCP
			modbusTcpBegin();
	#endif
	#ifdef IEC61850
			iec61850Reset();
	#endif
	displayBegin();
	cli_print_prompt();
}


void commandProcess() {
	int error;
	var_t *var = 0;
	char* name;
	unsigned int namelen;
	char* line = cli_readline();
	
	if( line == 0 )
		return;
	
	txtpos = line;
	parse_error = 0;
	
	ignore_blanks();
	
	//What this just a blank line
	if( *txtpos == 0 )
		goto command_done;
	
	//Parse Simple expression evaluations
	if( *txtpos == '?' ) {
		val_t a;
		txtpos++;
		if( ! expr_eval(&a) )
			goto interp_error;
		cli_print_val(a);
		goto command_done;
	}
	
	//Parse a name (could be a command)
	parse_name();
	if( next == txtpos ) {
		goto interp_error;
	}
	name = txtpos;
	namelen = next-txtpos;
	txtpos = next;
	ignore_blanks();
	
	if( parse_command(name,namelen) ) {
		if( parse_error )
			goto interp_error;
		goto command_done;
	}
	
	//This is variable with no assignment or point spec
	//Let's assume it's a mistyped command
	if( *txtpos == 0 ) {
		goto interp_error;
	}
	
	//Treat this as a variable
	var = get_var(name,namelen);
	if( ! var ) {
		var = make_var(name,namelen);
		if( ! var ) {
			goto interp_error;
		}
	}

	while( *txtpos != 0 ) {
		if( *txtpos == '=' ) {
			var = parse_assignment(var);
		} else if( *txtpos == ':' ) {
			var = parse_pointvar(var);
		} else {
			del_var(var);
			var = 0;
		}
		if( ! var ) {
			goto interp_error;
		}
		ignore_blanks();
	}
	goto command_done;
	
	interp_error:
		cli_print_interp_error(txtpos-line+1);
	command_done:
		cli_print_prompt();
		return;
}

