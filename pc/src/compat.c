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
#ifdef __DJGPP__
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include "dos/serial.h"
#include "cli.h"
#define COMMODE_NONE   0
#define COMMODE_SINGLE 1
#define COMMODE_DUAL   2
unsigned char commode;
#endif //__DJGPP__

#ifdef LINUX
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <ncurses.h>
#include <termios.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/time.h>
#include "cli.h"
WINDOW* console;
int comfd;
int servfd;
unsigned char readByte;
unsigned char readByteValid;
#endif //LINUX

#include "compat.h"

static void linuxUsage(char* cmd) {
	printf("Usage:\n");
	printf("%s [-h] [[-s serial_device] | [-t tcp_port]] [-f script]\n",cmd);
	printf("\n");
	printf("-s: Optionally specify serial port for SCADA communications\n");
	printf("-t: Optionally specify TCP server port to use for SCADA communications\n");
	printf("-f: Optionally specify script to run\n");
	printf("\n");
	exit(1);
}

static void dosUsage(char* cmd) {
	printf("Usage:\n");
	printf("%s [-h] [-s 0|1|2] [-f script]\n",cmd);
	printf("\n");
	printf("-s: Optionally specify the number of serial ports used.\n");
	printf("  0: No COM ports used\n");
	printf("  1: COM1 is SCADA\n");
	printf("  2: COM1 is console; COM2 is SCADA (default)\n");
	printf("-f: Optionally specify script to run\n");
	printf("\n");
	exit(1);
}

void compatBegin(int argc, char** argv) {	
	#ifdef ARDUINO
	randomSeed(analogRead(0));
	Serial.begin(9600);
	#ifndef MINI
	Serial2.begin(9600);
	#endif //MINI
	#else //ARDUINO
	srandom(time(0));
	#endif //ARDUINO
	
	#ifdef LINUX
	struct termios tty;
	int i = 1;
	comfd = -1;
	servfd = -1;
	while( i < argc ) {
		if( strcmp(argv[i],"-h") == 0 ) {
			linuxUsage(argv[0]);
		}
		else if( strcmp(argv[i],"-s") == 0 ) {
			if( i > argc-1 || servfd != -1 || comfd != -1 ) {
				linuxUsage(argv[0]);
			}
			comfd = open(argv[++i],O_RDWR|O_NONBLOCK);
			if( (comfd < 0) || (tcgetattr(comfd,&tty) != 0) ) {
				printf("Failed to open serial device: %s\n",argv[1]);
				exit(1);
			}
			//Configure as 9600 8,N,1 no flow control or special chars
			cfsetispeed(&tty,B9600);
			cfsetospeed(&tty,B9600);
			tty.c_cflag &= ~CSIZE;
			tty.c_cflag |= CS8;      //8 bits per byte
			tty.c_cflag &= ~PARENB;  //No parity
			tty.c_cflag &= ~CSTOPB;  //1 stop bit
			tty.c_cflag &= ~CRTSCTS; //no flow control
			tty.c_cflag |= CREAD|CLOCAL;
			tty.c_lflag &= ~ICANON;
			tty.c_lflag &= ~ECHO;
			tty.c_lflag &= ~ISIG;
			tty.c_iflag &= ~(IXON | IXOFF | IXANY);
			tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);
			tty.c_oflag &= ~OPOST;
			tty.c_oflag &= ~ONLCR;
			if( tcsetattr(comfd,TCSANOW, &tty) != 0 ) {
				printf("Failed to configure serial device: %s\n",argv[1]);
				exit(1);
			}
		}
		else if( strcmp(argv[i],"-t") == 0 ) {
			struct sockaddr_in addr;
			if( i > argc-1 || servfd != -1 || comfd != -1 ) {
				linuxUsage(argv[0]);
			}
			servfd = socket(AF_INET,SOCK_STREAM,0);
			if( servfd < 0 ) {
				printf("Failed to create TCP server socket.\n");
				exit(1);
			}
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = INADDR_ANY;
			addr.sin_port = htons(atoi(argv[++i]));
			if( bind( servfd, (struct sockaddr*)&addr, sizeof(addr)) < 0 ) {
				printf("Failed to bind TCP server to port: %s\n",argv[i]);
				exit(1);
			}
			if( listen(servfd,1) < 0 ) {
				printf("Failed to listen to TCP server.\n");
				exit(1);
			}
		}
		else if( strcmp(argv[i],"-f") ==0 ) {
			if( i > argc-1 ) {
				linuxUsage(argv[0]);
			}
			cli_start_load(argv[++i]);
		}
		else {
			linuxUsage(argv[0]);
		}
		i++;
	}
	readByteValid = 0;
	initscr();
	cbreak();
	noecho();
	console = newwin(getmaxy(stdscr),getmaxx(stdscr),0,0);
	nodelay(console,TRUE);
	scrollok(console,TRUE);
	#endif //LINUX

	#ifdef __DJGPP__
	commode = COMMODE_DUAL;
	int i = 1;
	while( i < argc ) {
		if( argv[i][0] != '-' || argv[i][2] != 0 ) {
			dosUsage(argv[0]);
		}
		else if( argv[i][1] == 'h' ) {
			dosUsage(argv[0]);
		}
		else if( argv[i][1] == 's' ) {
			if( i > argc-1 ) {
				dosUsage(argv[0]);
			}
			i++;
			if( argv[i][1] != 0 ) {
				dosUsage(argv[0]);
			}
			if( argv[i][0] == '0' ) {
				commode = COMMODE_NONE;
			} else if( argv[1][0] == '1' ) {
				commode = COMMODE_SINGLE;
			} else if( argv[1][0] == '2' ) {
				commode = COMMODE_DUAL;
			} else {
				dosUsage(argv[0]);
			}
		}
		else if( argv[i][1] == 'f' ) {
			if( i > argc-1 ) {
				dosUsage(argv[0]);
			}
			i++;
			cli_start_load(argv[i]);
		}
		else {
			dosUsage(argv[0]);
		}
		i++;
	}
	if( commode != COMMODE_NONE )
		serial_open(COM_1,9600,8,'n',1, SER_HANDSHAKING_NONE);
	if( commode == COMMODE_DUAL )
		serial_open(COM_2,9600,8,'n',1, SER_HANDSHAKING_NONE);
	#endif //__DJGPP__
}

unsigned int compatMillis() {
#ifdef ARDUINO
	return millis();
#endif //ARDUINO

#ifdef LINUX
	struct timespec tv;
	clock_gettime(CLOCK_MONOTONIC,&tv);
	return tv.tv_sec*1000+tv.tv_nsec/1000000;
#endif //LINUX

#ifdef __DJGPP__
	return uclock()*1000/(UCLOCKS_PER_SEC);
#endif
}

int compatRandom(int s, int e) {
#ifdef ARDUINO
	return random(s,e);
#else
	return s + (random()%(e-s));
#endif //ARDUINO
}

void compatExit() {
	#ifdef LINUX
	delwin(console);
	endwin();
	if( comfd != -1 ) {
		close(comfd);
	}
	#endif //LINUX
	
	#ifdef __DJGPP__
	serial_close(COM_1);
	serial_close(COM_2);
	#endif //__DJGPP__
	
	exit(0);
}

int scadaAvailable() {
	#ifdef ARDUINO
		#ifdef MINI
		return Serial.available();
		#else
		return Serial2.available();
		#endif //MINI
	#endif //ARDUINO
	
	#ifdef LINUX
	fd_set rfds;
	struct timeval timeout;
	if( servfd != -1 ) {
		//TCP SCADA channel
		if( comfd == -1 ) {
			//SCADA channel is not yet connected, try 
			//to accept a connection
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
			} else {
				return 0;
			}
		} else {
			return 0;
		}
	} else if( comfd != 1 ) {
		//COM SCADA channel
		if( readByteValid ) {
			return 1;
		} else if( 1 == read(comfd,&readByte,1) ) {
			readByteValid = 1;
			return 1;
		} else {
			return 0;
		}
	} else {
		//No SCADA channel
		return 0;
	}
	#endif //LINUX
	
	#ifdef __DJGPP__
		if( commode == COMMODE_DUAL )
			return serial_get_rx_buffered(COM_2);
		else if( commode == COMMODE_SINGLE )
			return serial_get_rx_buffered(COM_1);
		else
			return 0;
	#endif //__DJGPP__
}

int scadaReadByte() {
	#ifdef ARDUINO
	#ifdef MINI
		return Serial.read();
	#else
		return Serial2.read();
	#endif //MINI
	#endif //ARDUINO
	
	#ifdef LINUX
	if( servfd != -1 ) {
		//TCP SCADA channel
		if( scadaAvailable() ) {
			if( 1 == recv( comfd, &readByte, 1, 0 ) ) {
				readByteValid = 1;
			}
		}
	} else if( comfd != -1 ) {
		//COM SCADA channel
		if( ! readByteValid ) {
			if( 1 == read(comfd,&readByte,1) ) {
				readByteValid = 1;
			}
		}
	} else {
		//No SCADA channel
		readByteValid = 0;
	}

	//Return a valid byte if not exists
	if( readByteValid ) {
		readByteValid = 0;
		return readByte;
	} else {
		return -1;
	}
	#endif //LINUX

	#ifdef __DJGPP__
	char tmp;
	if( commode == COMMODE_DUAL  && serial_read(COM_2,&tmp,1) ) {
		return (int)tmp;
	} else if( commode == COMMODE_SINGLE && serial_read(COM_1,&tmp,1) ) {
		return (int)tmp;
	} else {
		return -1;
	}
	#endif //__DJGPP__
}

int scadaWriteMsgWithCRC(uint8_t *msg, uint16_t msg_len, uint16_t crc) {
	#ifdef ARDUINO
	#ifdef MINI
	Serial.write(msg,msg_len);
	Serial.write((crc&0xFF));
	Serial.write((crc>>8)&0xFF);
	#else
	Serial2.write(msg,msg_len);
	Serial2.write((crc&0xFF));
	Serial2.write((crc>>8)&0xFF);
	#endif //MINI
	#endif //ARDUINO
	
	#ifdef LINUX
		//#if __BYTE_ORDER == __BIG_ENDIAN
		//crc = ((crc&0x00FF)<<8) | ((crc&0xFF00)>>8)
		//#endif
	while( (-1 == write(comfd,msg,msg_len)) && (errno == EAGAIN) );
	while( (-1 == write(comfd,&crc,2)) && (errno == EAGAIN) );
	#endif //LINUX
	
	#ifdef __DJGPP__
	uint8_t crcByte;
	if( commode == COMMODE_DUAL ) {
		serial_write(COM_2,msg,msg_len);
		serial_write(COM_2,(char*)&crc,2);
	} else if( commode == COMMODE_SINGLE ) {
		serial_write(COM_1,msg,msg_len);
		serial_write(COM_1,(char*)&crc,2);
	}
	#endif //__DJGPP__
	
	return msg_len+2;
}

void consoleOut(char c) {
	#ifdef LINUX
	if( c == 0x7F ) {
		if( getcurx(console) != 1 ) {
			wmove(console,getcury(console),getcurx(console)-1);
			wdelch(console);
		}
	}
	else {
		waddch(console,c);
	}
	wrefresh(console);
	#endif //LINUX
	
	#ifdef __DJGPP__
	//Output to both VGA console and COM1
	int outc;
	if( c == '\r' ) {
		putch(c);
		putch('\n');
		if( commode == COMMODE_DUAL ) {
			outc = 0x0d0a;
			serial_write(COM_1,(char*)&outc,2);
		}
	} else if( c == '\n') {
		putch('\r');
		putch(c);
		if( commode == COMMODE_DUAL ) {
			outc = 0x0d0a;
			serial_write(COM_1,(char*)&outc,2);
		}
	} else {
		putch(c);
		if( commode == COMMODE_DUAL ) {
			serial_write(COM_1,(char*)&c,1);
		}
	}
	#endif //__DJGPP__
}

int consoleIn() {
	#ifdef LINUX
	return wgetch(console);
	#endif //LINUX
	
	#ifdef __DJGPP__
	//Input from either VGA console or COM1
	int c = -1;
	if( kbhit() ) {
		c = getch();
	}
	else if( commode == COMMODE_DUAL && serial_get_rx_buffered(COM_1) ) {
		c = 0;
		serial_read(COM_1,(char*)&c,1);
	}
	return c;
	#endif //__DJGPP
}

void displayOut(char c) {
	#ifdef LINUX
	int outc;
	if( c == '\n') {
		outc = 0x0d0a;
		fwrite(&outc,1,2,stdout);
	} else {
		fwrite(&c,1,1,stdout);
	}
	fflush(stdout);
	//waddch(console,c);
	#endif //LINUX

	#ifdef __DJGPP__
	putchar(c);
	#endif //__DJGPP__
}
