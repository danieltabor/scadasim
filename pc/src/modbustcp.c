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
#define __MODBUSTCP_C__
#include "modbustcp.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/time.h>

#include "compat.h"
#include "modbus.h"

#define MODBUSMSGLEN 1024
#define RXTIMEOUT 250

uint16_t modbustcp_port = 0;

static uint8_t req[MODBUSMSGLEN];
static uint16_t req_len;
static uint8_t res[MODBUSMSGLEN];
static uint16_t res_len;
static unsigned long last_recv;

static int servfd = -1;
static int comfd	= -1;

static int byteAvailable() {
	fd_set rfds;
	struct timeval timeout;
	if( servfd == -1 ) {
		return 0;
	}
	if( comfd == -1 ) {
		//Attempt to accept a connection
		FD_ZERO(&rfds);
		FD_SET(servfd,&rfds);
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		if( select( servfd+1, &rfds, 0, 0, &timeout ) ) {
			comfd = accept(servfd,0,0);
		}
	}
	if( comfd != -1 ) {
		//SCADA channel is connected
		FD_ZERO(&rfds);
		FD_SET(comfd,&rfds);
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		if( select(comfd+1, &rfds, 0, 0, &timeout ) ) {
			return 1;
		}
		else {
			return 0;
		}
	}
	return 0;
}

static int inputRequest() {
	int c;
	uint16_t protocol_id;
	uint16_t length;
	if( comfd == -1 ) {
		return 0;
	}
	if( 1 != recv( comfd, &c, 1, 0 ) ) {
		//a recv of zero bytes indicates a closed socket
		close(comfd);
		comfd = -1;
		req_len = 0;
		return 0;
	}
	if( compatMillis() - last_recv > RXTIMEOUT ) {
		req_len = 0;
	}
	last_recv = compatMillis();
	req[req_len++] = (int8_t) c;
	if( req_len == MODBUSMSGLEN ) {
		req_len = 0;
	}
	if( req_len >= 8 ) {
		length = (req[4]<<8) | req[5];
		if( req_len == length + 6 ) {
			protocol_id = (req[2] << 8) | req[3];
			if( protocol_id == 0 ) {
				return 1;
			}
			else {
				req_len = 0;
				return 0;
			}
		}
	}
	return 0;
}

static void modbusTcpReset() {
	if( comfd != -1 ) {
		close(comfd);
		comfd = -1;
	}
	if( servfd != -1 ) {
		close(servfd);
		servfd = -1;
	}
	
	req_len = 0;
	res_len = 0;
	last_recv = 0;
}

void modbusTcpBegin() {
	modbusTcpReset();
	modbustcp_port = 0;
}

int modbusTcpServ() {
	struct sockaddr_in addr;
	modbusTcpReset();
	
	if( modbustcp_port == 0 ) {
		return -1;
	}

	servfd = socket(AF_INET,SOCK_STREAM,0);
	if( servfd < 0 ) { 
		return -1; 
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(modbustcp_port);
	if( bind( servfd, (struct sockaddr*)&addr, sizeof(addr)) < 0 ) {
		return -1;
	}
	if( listen(servfd,1) < 0 ) {
		return -1;
	}
	return 0;
}


void modbusTcpProcess() {
	while( byteAvailable() ) {
		if( inputRequest() ) {
			modbusProcessRequest(req+6,res+6,&res_len);
			//printf("ModbusTCP Request: ");
			//for( int i=0; i<req_len; i++ ) {
			//	printf("%02X ",req[i]);
			//}
			//printf("\r\n");
			res[0] = req[0]; //Transation ID
			res[1] = req[1];
			res[2] = req[2]; //Protocol ID (already verified)
			res[3] = req[3]; 
			res[4] = (res_len & 0xFF00)>>8;
			res[5] = (res_len & 0x00FF);
			res[6] = req[6]; //unit id (ignored)
			res_len = res_len + 6;
			
			//Send the response
			//printf("Modbus Response: ");
			//for( int i=0; i<res_len; i++ ) {
			//	printf("%02X ",res[i]);
			//}
			//printf("\r\n");
			if( send(comfd,res,res_len,0) < 0 ) {
				close(comfd);
				comfd = -1;
			}
			req_len = 0;
		}
	}
}
