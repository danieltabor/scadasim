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
#include "iec61850_client.h"
#include "goose_receiver.h"
#include "goose_subscriber.h"

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <ncurses.h>
#include <pthread.h>

#define REPORT_DOMAIN "simpanelGenericIO"
#define POINT_DOMAIN  "simpanelGenericIO"
#define GOOSE_IFACE   "enp0s31f6"
#define GOOSE_EVENTS_APPID 1000
#define GOOSE_MEASUREMENTS_APPID 1001
int8_t GOOSE_DSTMAC[6] = {0x01,0x0c,0xcd,0x01,0x00,0x00};

#ifdef USE_CURSES
WINDOW* console = 0;
WINDOW* messages = 0;
WINDOW* control = 0;
#endif

#ifdef USE_SIXEL
extern int sixelDisplay();
#endif

#ifndef USE_CURSES
#ifndef USE_SIXEL
#define PRINT_MESSAGES
#endif
#endif


int running = 1;
int paused = 0;
//pthread_mutex_t lock;

char* dnames[] = {
	"ISO1",
	"ISO2",
	"BRK3",
	"BRK4",
	"ISO5",
	"ISO6",
	"BRK7",
	"ISO8"
};
char* dentrynames[] = {
	POINT_DOMAIN"/GGIO1.SPCSO1",
	POINT_DOMAIN"/GGIO1.SPCSO2",
	POINT_DOMAIN"/GGIO1.SPCSO3",
	POINT_DOMAIN"/GGIO1.SPCSO4",
	POINT_DOMAIN"/GGIO1.SPCSO5",
	POINT_DOMAIN"/GGIO1.SPCSO6",
	POINT_DOMAIN"/GGIO1.SPCSO7",
	POINT_DOMAIN"/GGIO1.SPCSO8"
};
int dv[8];

char* anames[] = {
	"BB1V",
	"BB2V",
	"BRK3V",
	"BRK3I",
	"BRK7V",
	"BRK7I",
	"BB3V",
	"BB4V"
};
char* aentrynames[] = {
	POINT_DOMAIN"/GGIO1.AnIn1",
	POINT_DOMAIN"/GGIO1.AnIn2",
	POINT_DOMAIN"/GGIO1.AnIn3",
	POINT_DOMAIN"/GGIO1.AnIn4",
	POINT_DOMAIN"/GGIO1.AnIn5",
	POINT_DOMAIN"/GGIO1.AnIn6",
	POINT_DOMAIN"/GGIO1.AnIn7",
	POINT_DOMAIN"/GGIO1.AnIn8",
};
float av[8];

#ifndef USE_CURSES

#ifdef __linux__
#define TIOCGETA TCGETS
#define TIOCSETA TCSETS
#endif

#include<unistd.h>
#include<fcntl.h>
#include<sys/ioctl.h>
#include<termios.h>
#include<errno.h>

int setStdinNonblocking() {
	int flags;
	//Use fcntl to make stdin NON-BLOCKING
	if( (flags = fcntl(STDIN_FILENO, F_GETFL, 0)) < 0 ) {
		fprintf(stderr,"Could not get fnctl flags: %s\n",strerror(errno));
		exit(1);
	}
	flags |= O_NONBLOCK;
	if( fcntl(STDIN_FILENO, F_SETFL, flags ) < 0 ) { 
		fprintf(stderr,"Could not set fnctl flags: %s\n",strerror(errno));
		exit(1);
	}
}

void resetStdinNonblocking(int flags) {
	//Use fcntl to reset stdin NON-BLOCKING
	flags &= ~O_NONBLOCK;
	if( fcntl(STDIN_FILENO, F_SETFL, flags ) < 0 ) { 
		fprintf(stderr,"Could not set fnctl flags: %s\n",strerror(errno));
		exit(1);
	}
}

void initStdin() {
	struct termios tios;
	//Use ioctl/TCGETS to disable echo and line buffering (icannon)
	if( ioctl(STDIN_FILENO, TIOCGETA, &tios)< 0 ) {
		fprintf(stderr,"Could not get termios flags: %s\n",strerror(errno));
		exit(1);
	}
	tios.c_lflag &= ~ECHO;
	tios.c_lflag &= ~ICANON;
	if( ioctl(STDIN_FILENO, TIOCSETA, &tios) < 0 ) {
		fprintf(stderr,"Could not set termios flags: %s\n",strerror(errno));
		exit(1);
	}
	//sixel if very sensative to certain console settings.
	//O_NONBLOCKING is one (for some unknown reason)
	#ifndef USE_SIXEL
	setStdinNonblocking();
	#endif
	
}

int getchImm() {
	ssize_t result;
	char input_char;
	#ifdef USE_SIXEL
	int flags = setStdinNonblocking();
	#endif
	
	result = read(STDIN_FILENO,&input_char,1);
	if( result < 0 ) {
		if( errno != EAGAIN ) {
			printf("Failed to read stdin: %s\n",strerror(errno));
			exit(1);
		}
	}
	
	#ifdef USE_SIXEL
	resetStdinNonblocking(flags);
	#endif
	
	if( result == 0 ) {
		return ERR;
	} else {
		return input_char;
	}
}
void resetStdin() {
	system("stty sane");
}
#endif


void
sigint_handler(int signalId) {
	running = 0;
}

void 
processEventsRecursive( char* entryName, MmsValue* value, int level ) {
	unsigned int i;
	unsigned int nidx;
	char buffer[256];
	
	if( value == 0 ) {
		return;
	}
	if( MmsValue_getType(value) == MMS_STRUCTURE || 
		MmsValue_getType(value) == MMS_ARRAY ) {
		for( i=0; i<MmsValue_getArraySize(value); i++ ) {
			processEventsRecursive(entryName, MmsValue_getElement(value,i), level+1);
		}
	}
	else {
		//Handle Single Indicators
		if( MmsValue_getType(value) == MMS_BOOLEAN ) {
			for( nidx=0; nidx<8; nidx++ ) {
				if( strncmp(dentrynames[nidx],entryName,strlen(dentrynames[nidx])) == 0 ) {
					if( MmsValue_getBoolean(value) ) {
						dv[nidx] = 1;
					}
					else {
						dv[nidx] = 0;
					}
					break;
				}
			}
		}
		//Handle Double Indicators
		else if( 	MmsValue_getType(value) == MMS_BIT_STRING &&
					MmsValue_getBitStringSize(value) == 2 ) {
			for( nidx=0; nidx<8; nidx++ ) {
				if( strncmp(dentrynames[nidx],entryName,strlen(dentrynames[nidx])) == 0 ) {
					if( MmsValue_getBitStringBit(value,0) && 
						! MmsValue_getBitStringBit(value,1) ) {
						dv[nidx] = 1;
					}
					else if( ! MmsValue_getBitStringBit(value,0) &&
								MmsValue_getBitStringBit(value,1) ) {
						dv[nidx] = 0;
					}
					else {
						dv[nidx] = 1;
					}
					break;
				}
			}
		}
		#ifdef USE_CURSES
		//Print Message/Vaue Info
		if( level ) {
			wprintw(messages,"|");
			for( i=1; i<level; i++ ) {
				wprintw(messages,"-");
			}
		}
		MmsValue_printToBuffer(value,buffer,256);
		wprintw(messages,"(%s) %s\n",MmsValue_getTypeString(value),buffer);
		#endif
		
		#ifdef PRINT_MESSAGES
		if( level ) {
			printf("|");
			for( i=1; i<level; i++ ) {
				printf("-");
			}
		}
		MmsValue_printToBuffer(value,buffer,256);
		printf("(%s) %s\n",MmsValue_getTypeString(value),buffer);
		#endif
	}
}

void
processEventsReport(void* parameter, ClientReport report) {
	unsigned int i;
	MmsValue* dataSetValues = ClientReport_getDataSetValues(report);
	LinkedList dataSetDirectory = (LinkedList)parameter;
	LinkedList entry;
	char* entryName;
	
	if( paused ) {
		return;
	}
	
	//pthread_mutex_lock(&lock);
	
	#ifdef USE_CURSES
	wprintw(messages,"Events Report:\n");
	#endif
		
	#ifdef PRINT_MESSAGES
	printf("Events Report:\n");
	#endif
	
	for( i=0; i<MmsValue_getArraySize(dataSetValues); i++) {
		entry = LinkedList_get(dataSetDirectory, i);
		entryName = (char*) entry->data;
		ReasonForInclusion reason = ClientReport_getReasonForInclusion(report, i);
		if( reason == IEC61850_REASON_NOT_INCLUDED ) {
			#ifdef USE_CURSES
			//wprintw(messages,"%s: not included\n",entryName);
			#endif
		
			#ifdef PRINT_MESSAGES
			//printf("%s: not included\n",entryName);
			#endif
		}
		else {
			#ifdef USE_CURSES
			wprintw(messages,"%s:\n",entryName);
			#endif
		
			#ifdef PRINT_MESSAGES
			printf("%s:\n",entryName);
			#endif
			processEventsRecursive(entryName,MmsValue_getElement(dataSetValues,i),1);
		}
	}
	
	#ifdef USE_CURSES
	wprintw(messages,"\n");
	wrefresh(messages);
	#endif
	
	#ifdef PRINT_MESSAGES
	printf("\n");
	#endif
	
	//pthread_mutex_unlock(&lock);
}

void
processEventsGoose(GooseSubscriber subscriber, void* parameter) {
	unsigned int i;
	MmsValue* dataSetValues = GooseSubscriber_getDataSetValues(subscriber);
	LinkedList dataSetDirectory = (LinkedList)parameter;
	LinkedList entry;
	char* entryName;
	
	if( paused ) {
		return;
	}
	
	//pthread_mutex_lock(&lock);
	
	#ifdef USE_CURSES
	wprintw(messages,"Events GOOSE:\n");
	#endif
		
	#ifdef PRINT_MESSAGES
	printf("Events GOOSE:\n");
	#endif
	
	for( i=0; i<MmsValue_getArraySize(dataSetValues); i++) {
		entry = LinkedList_get(dataSetDirectory, i);
		entryName = (char*) entry->data;
		#ifdef USE_CURSES
		wprintw(messages,"%s:\n",entryName);
		#endif
		
		#ifdef PRINT_MESSAGES
		printf("%s:\n",entryName);
		#endif
		processEventsRecursive(entryName,MmsValue_getElement(dataSetValues,i),1);
	}
	
	#ifdef USE_CURSES
	wprintw(messages,"\n");
	wrefresh(messages);
	#endif
	
	#ifdef PRINT_MESSAGES
	printf("\n");
	#endif
	
	//pthread_mutex_unlock(&lock);
}


void 
processMeasurementsRecursive( char* entryName, MmsValue* value, int level ) {
	unsigned int i;
	unsigned int nidx;
	char buffer[256];
	
	if( value == 0 ) {
		return;
	}
	if( MmsValue_getType(value) == MMS_STRUCTURE || 
		MmsValue_getType(value) == MMS_ARRAY ) {
		for( i=0; i<MmsValue_getArraySize(value); i++ ) {
			processMeasurementsRecursive(entryName, MmsValue_getElement(value,i), level+1);
		}
	}
	else {
		//Handle Analog Floats
		if( MmsValue_getType(value) == MMS_FLOAT ) {
			for( nidx=0; nidx<8; nidx++ ) {
				if( strncmp(aentrynames[nidx],entryName,strlen(aentrynames[nidx])) == 0 ) {
					av[nidx] = MmsValue_toFloat(value);
					break;
				}
			}
		}
		
		#ifdef USE_CURSES
		//Print Message/Vaue Info
		if( level ) {
			wprintw(messages,"|");
			for( i=1; i<level; i++ ) {
				wprintw(messages,"-");
			}
		}
		MmsValue_printToBuffer(value,buffer,256);
		wprintw(messages,"(%s) %s\n",MmsValue_getTypeString(value),buffer);
		#endif
		
		#ifdef PRINT_MESSAGES
		if( level ) {
			printf("|");
			for( i=1; i<level; i++ ) {
				printf("-");
			}
		}
		MmsValue_printToBuffer(value,buffer,256);
		printf("(%s) %s\n",MmsValue_getTypeString(value),buffer);
		#endif
	}
}


void
processMeasurementsReport(void* parameter, ClientReport report) {
	unsigned int i;
	MmsValue* dataSetValues = ClientReport_getDataSetValues(report);
	LinkedList dataSetDirectory = (LinkedList)parameter;
	LinkedList entry;
	char* entryName;
	
	if( paused ) {
		return;
	}
	
	//pthread_mutex_lock(&lock);
	
	#ifdef USE_CURSES
	wprintw(messages,"Measurements Report:\n");
	#endif
		
	#ifdef PRINT_MESSAGES
	printf("Measurements Report:\n");
	#endif
	
	for( i=0; i<MmsValue_getArraySize(dataSetValues); i++) {
		entry = LinkedList_get(dataSetDirectory, i);
		entryName = (char*) entry->data;
		ReasonForInclusion reason = ClientReport_getReasonForInclusion(report, i);
		if( reason == IEC61850_REASON_NOT_INCLUDED ) {
			#ifdef USE_CURSES
			//wprintw(messages,"%s: not included\n",entryName);
			#endif
		
			#ifdef PRINT_MESSAGES
			//printf("%s: not included\n",entryName);
			#endif
		}
		else {
			#ifdef USE_CURSES
			wprintw(messages,"%s:\n",entryName);
			#endif
		
			#ifdef PRINT_MESSAGES
			printf("%s:\n",entryName);
			#endif
			processMeasurementsRecursive(entryName,MmsValue_getElement(dataSetValues,i),1);
		}
	}
	
	#ifdef USE_CURSES
	wprintw(messages,"\n");
	wrefresh(messages);
	#endif
	
	#ifdef PRINT_MESSAGES
	printf("\n");
	#endif
	
	//pthread_mutex_unlock(&lock);
}

void
processMeasurementsGoose(GooseSubscriber subscriber, void* parameter) {
	unsigned int i;
	MmsValue* dataSetValues = GooseSubscriber_getDataSetValues(subscriber);
	LinkedList dataSetDirectory = (LinkedList)parameter;
	LinkedList entry;
	char* entryName;
	
	if( paused ) {
		return;
	}
	
	//pthread_mutex_lock(&lock);
	
	#ifdef USE_CURSES
	wprintw(messages,"Measurements GOOSE:\n");
	#endif
		
	#ifdef PRINT_MESSAGES
	printf("Measurements GOOSE:\n");
	#endif
	
	for( i=0; i<MmsValue_getArraySize(dataSetValues); i++) {
		entry = LinkedList_get(dataSetDirectory, i);
		entryName = (char*) entry->data;
		#ifdef USE_CURSES
		wprintw(messages,"%s:\n",entryName);
		#endif
		
		#ifdef PRINT_MESSAGES
		printf("%s:\n",entryName);
		#endif
		processMeasurementsRecursive(entryName,MmsValue_getElement(dataSetValues,i),0);
	}
	
	#ifdef USE_CURSES
	wprintw(messages,"\n");
	wrefresh(messages);
	#endif
	
	#ifdef PRINT_MESSAGES
	printf("\n");
	#endif
	
	//pthread_mutex_unlock(&lock);
}


static void
processDisconnect(void *parameter, IedConnection connection) {
	running = 0;
}

static void printRecursive(MmsValue* value, int level) {
	int len, i;
	char buffer[1024];
	for( i=0; i<level; i++ ) { printf("  "); }
	if( MmsValue_getType(value) == MMS_STRUCTURE ||
		MmsValue_getType(value) == MMS_ARRAY ) {
		printf("[%s]\n",MmsValue_getTypeString(value));
		len	= MmsValue_getArraySize(value);
		for( i=0; i<len; i++ ) {
			printRecursive(MmsValue_getElement(value,i),level+1);
		}
	}
	else {
		MmsValue_printToBuffer(value,buffer,1024);
		printf("[%s] %s\n",MmsValue_getTypeString(value),buffer);
	}
}

static void readRecursive(IedConnection con, char* path, char* constraint) {
	MmsValue* value;
	IedClientError error;
	 FunctionalConstraint fc;
	
	if( !strcasecmp(constraint,"st") ) { fc = IEC61850_FC_ST; }
	else if( !strcasecmp(constraint,"mx") ) { fc = IEC61850_FC_MX; }
	else if( !strcasecmp(constraint,"sp") ) { fc = IEC61850_FC_SP; }
	else if( !strcasecmp(constraint,"sv") ) { fc = IEC61850_FC_SV; }
	else if( !strcasecmp(constraint,"cf") ) { fc = IEC61850_FC_CF; }
	else if( !strcasecmp(constraint,"dc") ) { fc = IEC61850_FC_DC; }
	else if( !strcasecmp(constraint,"sg") ) { fc = IEC61850_FC_SG; }
	else if( !strcasecmp(constraint,"se") ) { fc = IEC61850_FC_SE; }
	else if( !strcasecmp(constraint,"sr") ) { fc = IEC61850_FC_SR; }
	else if( !strcasecmp(constraint,"or") ) { fc = IEC61850_FC_OR; }
	else if( !strcasecmp(constraint,"bl") ) { fc = IEC61850_FC_BL; }
	else if( !strcasecmp(constraint,"ex") ) { fc = IEC61850_FC_EX; }
	else if( !strcasecmp(constraint,"co") ) { fc = IEC61850_FC_CO; }
	else if( !strcasecmp(constraint,"us") ) { fc = IEC61850_FC_US; }
	else if( !strcasecmp(constraint,"ms") ) { fc = IEC61850_FC_MS; }
	else if( !strcasecmp(constraint,"rp") ) { fc = IEC61850_FC_RP; }
	else if( !strcasecmp(constraint,"br") ) { fc = IEC61850_FC_BR; }
	else if( !strcasecmp(constraint,"lg") ) { fc = IEC61850_FC_LG; }
	else if( !strcasecmp(constraint,"go") ) { fc = IEC61850_FC_GO; }
	else if( !strcasecmp(constraint,"all") )  { fc = IEC61850_FC_ALL; }
	else if( !strcasecmp(constraint,"none") )  { fc = IEC61850_FC_NONE; }
	else {
		printf("Unsupported functional constraint: %s\n",constraint);
		printf("Supported functional constraints:\n");
		printf("  mx, sp, sv, cf, dc, sg, se, sr, or\n");
		printf("  bl, ex, co, us, ms, rp, br, lg, go\n");
		printf("  all, none\n");
		printf("\n");
		return;
	}
	
	printf("Reading Recursive %s %s\n",path,constraint);
	value = IedConnection_readObject(con,&error,path,fc);
	if( value == NULL ) {
		printf("Failed with Error: %d\n",error);
		return;
	}
	printRecursive(value,0);
}

void getDataDirectoryRecursive(IedConnection con, char* parentname) {
	char* name;
	char fullname[256];
	int i;
	IedClientError error;
	LinkedList directory;
	FunctionalConstraint fc;
	
	printf("%s\n",parentname);
	directory = IedConnection_getDataDirectory(con,&error,parentname);
	if( error != IED_ERROR_OK ) {
		printf("Could not read directory (%d)\n",error);
		return;
	}
	for( i=0; i<LinkedList_size(directory); i++ ) {
		name = (char*)(LinkedList_get(directory,i)->data);
		snprintf(fullname,256,"%s.%s",parentname,name);
		getDataDirectoryRecursive(con,fullname);
	}
	LinkedList_destroyDeep(directory,free);
}

void getDataSetDirectory(IedConnection con, char* datasetname) {
	IedClientError error;
	LinkedList dataSetDirectory;
	char* ref;
	int i;
	
	printf("%s\n",datasetname);
	dataSetDirectory = IedConnection_getDataSetDirectory(con, &error, datasetname, NULL);
	if( error != IED_ERROR_OK ) {
		printf("Could not read data set (%d)\n",error);
		return;
	}
	for( i=0; i<LinkedList_size(dataSetDirectory); i++ ) {
		ref = (char*)(LinkedList_get(dataSetDirectory,i)->data);
		printf("  %s\n",ref);
	}
	LinkedList_destroyDeep(dataSetDirectory,free);
}

void getDirectoryWithAsci(IedConnection con, ACSIClass asciClass) {
	IedClientError error;
	char* devicename;
	LinkedList logical_devices;
	int ldidx;
	char* nodename;
	LinkedList logical_nodes;
	int lnidx;
	char* doname;
	LinkedList data_objects;
	int doidx;
	char fullname[256];

	IedConnection_getDeviceModelFromServer(con, &error);
	if( error != IED_ERROR_OK ) {
		printf("Could not load device model from server\n");
		return;
	}
	logical_devices = IedConnection_getLogicalDeviceList(con,&error);
	if( error != IED_ERROR_OK ) {
		printf("Failed to read Logical Device List.\n");
		return;
	}
	for( ldidx=0; ldidx<LinkedList_size(logical_devices); ldidx++ ) {
		devicename = (char*)(LinkedList_get(logical_devices,ldidx)->data);
		logical_nodes = IedConnection_getLogicalDeviceDirectory(con,&error,devicename);
		if( error != IED_ERROR_OK ) {
			printf("  Could not read logical nodes\n");
			continue;
		}
		for( lnidx=0; lnidx<LinkedList_size(logical_nodes); lnidx++ ) {
			nodename = (char*)(LinkedList_get(logical_nodes,lnidx)->data);
			snprintf(fullname,256,"%s/%s",devicename,nodename);
			//printf("%s\n",fullname);
			data_objects = IedConnection_getLogicalNodeDirectory(con,&error,fullname,asciClass);
			if( error != IED_ERROR_OK ) {
				printf("  Could not read node directory\n");
				return;
			}
			for( doidx=0; doidx<LinkedList_size(data_objects); doidx++ ) {
				doname = (char*)(LinkedList_get(data_objects,doidx)->data);
				snprintf(fullname,256,"%s/%s.%s",devicename,nodename,doname);
				if( asciClass == ACSI_CLASS_DATA_SET ) {
					getDataSetDirectory(con,fullname);
				}
				else {
					getDataDirectoryRecursive(con,fullname);
				}
			}
			LinkedList_destroyDeep(data_objects,free);
		}
	}
	LinkedList_destroyDeep(logical_nodes,free);
	LinkedList_destroyDeep(logical_devices,free);
}


void getDirectory(IedConnection con) {
	printf("## Data Sets ##\n");
	getDirectoryWithAsci(con,ACSI_CLASS_DATA_SET);
	printf("## Buffered Report Control Blocks ##\n");
	getDirectoryWithAsci(con,ACSI_CLASS_BRCB);
	printf("## Unbuffered Report Control Blocks ##\n");
	getDirectoryWithAsci(con,ACSI_CLASS_URCB);
	printf("## Logs ##\n");
	getDirectoryWithAsci(con,ACSI_CLASS_LOG);
	printf("## Log Control Blocks  ##\n");
	getDirectoryWithAsci(con,ACSI_CLASS_LCB);
	printf("## SgCB ##\n");
	getDirectoryWithAsci(con,ACSI_CLASS_SGCB);
	printf("## GsCB ##\n");
	getDirectoryWithAsci(con,ACSI_CLASS_GsCB);
	printf("## GoCB ##\n");
	getDirectoryWithAsci(con,ACSI_CLASS_GoCB);
	printf("## MsvCB ##\n");
	getDirectoryWithAsci(con,ACSI_CLASS_MSVCB);
	printf("## UsvCB ##\n");
	getDirectoryWithAsci(con,ACSI_CLASS_USVCB);
	printf("## Data Objects ##\n");
	getDirectoryWithAsci(con,ACSI_CLASS_DATA_OBJECT);
}


void usage(char* cmd) {
	printf("Usage:\n");
	printf("%s [-h] [-c hostname port] [-br | -g] [-sbo] [-dir | -r path fc]\n",cmd);
	printf("\n");
	printf("Interactive simpanel (default):\n");
	printf("  %s -c localhost 102\n",cmd);
	printf("-br : use buffered reports (defaults to unbuffered reports)\n");
	printf("-g  : use GOOSE for feedback (defaults to unbuffered reports)\n");
	printf("-sbo: select before operate (defaults to direct)\n");
	printf("-dir: get a full directory of the server's data model\n");
	printf("Recursive Read Example:\n");
	printf("  %s -c localhost 61850 -r simpanelGenericIO/GGIO1 st\n",cmd);
	printf("\n");
	exit(1);
}

int main(int argc, char** argv) {
	int i;
	char* hostname = "localhost";
	int tcpPort = 102;
	uint64_t current_time, next_console, next_control;
	int get_dir = 0;
	char* read_path = 0;
	char* read_fc = 0;
	int dvalue = 0;
	int use_buffered_reports = 0;
	int use_goose = 0;
	int select_before_operate = 0;

	i = 1;
	while( i<argc ) {
		if( strcmp(argv[i],"-c") == 0 ) {
			if( i >= argc-2 ) {
				usage(argv[0]);
			}
			hostname = argv[++i];
			tcpPort = atoi(argv[++i]);
		}
		else if( strcmp(argv[i],"-br") == 0 ) {
			if( use_goose ) {
				usage(argv[0]);
			}
			use_buffered_reports = 1;
		}
		else if( strcmp(argv[i],"-g") == 0 ) {
			if( use_buffered_reports ) {
				usage(argv[0]);
			}
			use_goose = 1;
		}
		else if( strcmp(argv[i],"-sbo") == 0 ) {
			select_before_operate = 1;
		}
		else if( strcmp(argv[i],"-dir") == 0 ) {
			if( read_path != 0 ) {
				usage(argv[0]);
			}
			get_dir = 1;
		}
		else if( strcmp(argv[i],"-r") == 0 ) {
			if( i >= argc-2 ||
				get_dir         ) {
				usage(argv[0]);
			}
			read_path = argv[++i];
			read_fc = argv[++i];
		}
		else {
			usage(argv[0]);
		}
		++i;
	}

	for( i=0; i<8; i++ ) {
		dv[i] = -1;
	}
	for( i=0; i<8; i++ ) {
		av[i] = -1;
	}

	IedClientError error;
	LinkedList dataSetDirectory;
	ClientReportControlBlock rcb;
	GooseReceiver receiver;
	GooseSubscriber subscriberEvents;
	GooseSubscriber subscriberMeasurements;

	IedConnection con = IedConnection_create();

	IedConnection_connect(con, &error, hostname, tcpPort);

	if (error != IED_ERROR_OK) {
		printf("Could not connect to %s:%d\n",hostname,tcpPort);
		return 1;
	}

	IedConnection_installConnectionClosedHandler(con,processDisconnect,0);

	if( get_dir ) {
		getDirectory(con);
		return 0;
	}

	if( read_path!= 0 ) {
		readRecursive(con,read_path,read_fc);
		IedConnection_close(con);
		IedConnection_destroy(con);
		return 0;
	}

	//pthread_mutex_init(&lock,0);
	signal(SIGINT, sigint_handler);
	
	//Initialize the curses display
	#ifdef USE_CURSES
	initscr();
	cbreak();
	noecho();
	console = newwin(17,36,0,0);
	messages = newwin(getmaxy(stdscr),getmaxx(stdscr)-36,0,36);
	control = newwin(4,36,18,0);
	nodelay(console,TRUE);
	scrollok(console,TRUE);
	nodelay(messages,TRUE);
	scrollok(messages,TRUE);
	nodelay(control,TRUE);
	scrollok(control,TRUE);
	#else
	initStdin();
	#endif
	
	/////////////////////////
	// Option A: Use GOOSE
	/////////////////////////
	if( use_goose ) {
		receiver = GooseReceiver_create();
		GooseReceiver_setInterfaceId(receiver,GOOSE_IFACE);
		
		//Handle Digital Signals
		dataSetDirectory = IedConnection_getDataSetDirectory(con, &error, REPORT_DOMAIN"/LLN0.dsEvents", NULL);
		if (error != IED_ERROR_OK) {
			#ifdef USE_CURSES
			endwin();
			#else
			resetStdin();
			#endif
			printf("Failed to read Data Set Directory for Event GOOSE\n");
			IedConnection_close(con);
			IedConnection_destroy(con);
			return 1;
		}
		
		subscriberEvents = GooseSubscriber_create(REPORT_DOMAIN"/LLN0$GO$gcbEvents",NULL);
		GooseSubscriber_setDstMac(subscriberEvents, GOOSE_DSTMAC);
		GooseSubscriber_setAppId(subscriberEvents,GOOSE_EVENTS_APPID);
		GooseSubscriber_setListener(subscriberEvents, processEventsGoose, dataSetDirectory);
		GooseReceiver_addSubscriber(receiver, subscriberEvents);
		
		//Handle Analog Signals
		dataSetDirectory = IedConnection_getDataSetDirectory(con, &error, REPORT_DOMAIN"/LLN0.dsMeasurements", NULL);
		if (error != IED_ERROR_OK) {
			#ifdef USE_CURSES
			endwin();
			#else
			resetStdin();
			#endif
			printf("Failed to read Data Set Directory for Measurements GOOSE\n");
			IedConnection_close(con);
			IedConnection_destroy(con);
			return 1;
		}
		
		subscriberMeasurements = GooseSubscriber_create(REPORT_DOMAIN"/LLN0$GO$gcbMeasurements",NULL);
		GooseSubscriber_setDstMac(subscriberMeasurements, GOOSE_DSTMAC);
		GooseSubscriber_setAppId(subscriberMeasurements,GOOSE_MEASUREMENTS_APPID);
		GooseSubscriber_setListener(subscriberMeasurements, processMeasurementsGoose, dataSetDirectory);
		GooseReceiver_addSubscriber(receiver, subscriberMeasurements);
		
		//Start the receiver - This will silently fail if not executed with proper priviledges
		GooseReceiver_start(receiver);
	}
	///////////////////////////////////
	// Option B: Use Buffered Reports
	///////////////////////////////////
	else if( use_buffered_reports ) {
		//Handle Digital Signals
		dataSetDirectory = IedConnection_getDataSetDirectory(con, &error, REPORT_DOMAIN"/LLN0.dsEvents", NULL);
		if( error != IED_ERROR_OK ) {
			#ifdef USE_CURSES
			endwin();
			#else
			resetStdin();
			#endif
			printf("Failed to read Data Set Directory for Events\n");
			IedConnection_close(con);
			IedConnection_destroy(con);
			return 1;
		}
		
		rcb = IedConnection_getRCBValues(con, &error, REPORT_DOMAIN"/LLN0$BR$brcbEvents", NULL);
		if( error != IED_ERROR_OK ) {
			#ifdef USE_CURSES
			endwin();
			#else
			resetStdin();
			#endif
			printf("Failed to read Buffered Report Control Block for Events\n");
			IedConnection_close(con);
			IedConnection_destroy(con);
			return 1;
		}

		ClientReportControlBlock_setRptEna(rcb, true);
		IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_RPT_ENA, true);
		if( error != IED_ERROR_OK ) {
			#ifdef USE_CURSES
			endwin();
			#else
			resetStdin();
			#endif
			printf("Failed to enable Buffered Report Control Block for Events\n");
			IedConnection_close(con);
			IedConnection_destroy(con);
			return 1;
		}
		
		IedConnection_installReportHandler(con, 
			REPORT_DOMAIN"/LLN0$BR$brEvents",
			ClientReportControlBlock_getRptId(rcb), processEventsReport, dataSetDirectory);

		//Handle Analog Values
		dataSetDirectory = IedConnection_getDataSetDirectory(con, &error, REPORT_DOMAIN"/LLN0.dsMeasurements", NULL);
		if( error != IED_ERROR_OK ) {
			#ifdef USE_CURSES
			endwin();
			#else
			resetStdin();
			#endif
			printf("Failed to read Data Set Directory for Measurements\n");
			IedConnection_close(con);
			IedConnection_destroy(con);
			return 1;
		}
		
		rcb = IedConnection_getRCBValues(con, &error, REPORT_DOMAIN"/LLN0$BR$brcbMeasurements", NULL);
		if( error != IED_ERROR_OK ) {
			#ifdef USE_CURSES
			endwin();
			#else
			resetStdin();
			#endif
			printf("Failed to read Buffered Report Control Block for Measurements\n");
			IedConnection_close(con);
			IedConnection_destroy(con);
			return 1;
		}

		ClientReportControlBlock_setRptEna(rcb, true);
		IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_RPT_ENA, true);
		if( error != IED_ERROR_OK ) {
			#ifdef USE_CURSES
			endwin();
			#else
			resetStdin();
			#endif
			printf("Failed to enable Buffered Report Control Block for Measurements\n");
			IedConnection_close(con);
			IedConnection_destroy(con);
			return 1;
		}

		IedConnection_installReportHandler(con,
			REPORT_DOMAIN"/LLN0$BR$brMeasurements",
			ClientReportControlBlock_getRptId(rcb), processMeasurementsReport, dataSetDirectory);
	}
	/////////////////////////////////////
	// Option C: Use Unbuffered Reports
	/////////////////////////////////////
	else {
		//Handle Digital Signals
		dataSetDirectory = IedConnection_getDataSetDirectory(con, &error, REPORT_DOMAIN"/LLN0.dsEvents", NULL);
		if( error != IED_ERROR_OK ) {
			#ifdef USE_CURSES
			endwin();
			#else
			resetStdin();
			#endif
			printf("Failed to read Data Set Directory for Events\n");
			IedConnection_close(con);
			IedConnection_destroy(con);
			return 1;
		}
		
		rcb = IedConnection_getRCBValues(con, &error, REPORT_DOMAIN"/LLN0$RP$urcbEvents", NULL);
		if( error != IED_ERROR_OK ) {
			#ifdef USE_CURSES
			endwin();
			#else
			resetStdin();
			#endif
			printf("Failed to read Unbuffered Report Control Block for Events\n");
			IedConnection_close(con);
			IedConnection_destroy(con);
			return 1;
		}

		ClientReportControlBlock_setRptEna(rcb, true);
		IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_RPT_ENA, true);
		if( error != IED_ERROR_OK ) {
			#ifdef USE_CURSES
			endwin();
			#else
			resetStdin();
			#endif
			printf("Failed to enable Unbuffered Report Control Block for Events\n");
			IedConnection_close(con);
			IedConnection_destroy(con);
			return 1;
		}
		
		IedConnection_installReportHandler(con, 
			REPORT_DOMAIN"/LLN0$RP$rpEvents",
			ClientReportControlBlock_getRptId(rcb), processEventsReport, dataSetDirectory);

		//Handle Analog Values
		dataSetDirectory = IedConnection_getDataSetDirectory(con, &error, REPORT_DOMAIN"/LLN0.dsMeasurements", NULL);
		if( error != IED_ERROR_OK ) {
			#ifdef USE_CURSES
			endwin();
			#else
			resetStdin();
			#endif
			printf("Failed to read Data Set Directory for Measurements\n");
			IedConnection_close(con);
			IedConnection_destroy(con);
			return 1;
		}
		
		rcb = IedConnection_getRCBValues(con, &error, REPORT_DOMAIN"/LLN0$RP$urcbMeasurements", NULL);
		if( error != IED_ERROR_OK ) {
			#ifdef USE_CURSES
			endwin();
			#else
			resetStdin();
			#endif
			printf("Failed to read Unbuffered Report Control Block for Measurements\n");
			IedConnection_close(con);
			IedConnection_destroy(con);
			return 1;
		}

		ClientReportControlBlock_setRptEna(rcb, true);
		IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_RPT_ENA, true);
		if( error != IED_ERROR_OK ) {
			#ifdef USE_CURSES
			endwin();
			#else
			resetStdin();
			#endif
			printf("Failed to enable Unbuffered Report Control Block for Measurements\n");
			IedConnection_close(con);
			IedConnection_destroy(con);
			return 1;
		}

		IedConnection_installReportHandler(con,
			REPORT_DOMAIN"/LLN0$RP$rpMeasurements",
			ClientReportControlBlock_getRptId(rcb), processMeasurementsReport, dataSetDirectory);
	}
		
	//Run the main display loop while async report handlers update the data and show messages in the background.
	next_console = Hal_getTimeInMs();
	next_control = next_console;
	while( running ) {
		current_time = Hal_getTimeInMs();
		#ifdef USE_CURSES
		i = wgetch(console);
		#else
		i = getchImm();
		#endif
		if( i != ERR ) {
			//pthread_mutex_lock(&lock);
			if( i == 'q' || i == 'Q' ) {
				break;
			}
			else if( i == ' ' ) {
				if( paused ) {
					#ifdef USE_CURSES
					wclear(control);
					wrefresh(control);
					#endif
					paused = 0;
				} else {
					paused = 1;
					#ifdef USE_CURSES
					mvwprintw(control,3,5,"PAUSED");
					wrefresh(control);
					#else
					printf("PAUSED\n");
					#endif
				}
			}
			else if( !paused && current_time >= next_control && i >= '1' && i <= '8' ) {
				char name[256];
				i = i - '0';
				MmsValue *ctlVal;
				ControlObjectClient clientControl;
				
				snprintf(name,256,POINT_DOMAIN"/GGIO1.SPCSO%d",i);
				if( dv[i-1] == -1 || dv[i-1] == 2 ) {
					dvalue = 1 - dvalue;
				} else {
					dvalue = 1 - dv[i-1];
				}
				ctlVal = MmsValue_newBoolean(dvalue);
				
				#ifdef USE_CURSES
				wclear(control);
				mvwprintw(control,0,0,"Ctrl %s=%d",dnames[i-1],dvalue);
				mvwprintw(control,1,2,"%s",name);
				#else
				printf("Ctrl %s=%d\n",dnames[i-1],dvalue);
				printf(" %s\n",name);
				#endif
				
				clientControl = ControlObjectClient_create(name, con);
				if( clientControl == NULL ) {
					#ifdef USE_CURSES
					endwin();
					#else
					resetStdin();
					#endif
					printf("Unable to Create control\n");
					break;
				}
				
				ControlObjectClient_setOrigin(clientControl, NULL, 3);
				if( select_before_operate ) {
					if( ControlObjectClient_selectWithValue(clientControl,ctlVal) ) {
						#ifdef USE_CURSES
						mvwprintw(control,2,2,"Select Success");
						#else
						printf("  Select Success\n");
						#endif
						if( ControlObjectClient_operate(clientControl,ctlVal,0) ) {
							#ifdef USE_CURSES
							mvwprintw(control,3,2,"Operate Success");
							#else
							printf("  Operate Success\n");
							#endif
						} else {
							#ifdef USE_CURSES
							mvwprintw(control,3,2,"Operate Failed");
							#else
							printf("  Operate Failed\n");
							#endif
						}
					} else {
						#ifdef USE_CURSES
						mvwprintw(control,2,2,"Select Failed");
						#else
						printf("  Select Failed\n");
						#endif
					}
				} else {	
					if( ControlObjectClient_operate(clientControl, ctlVal, 0) ) {
						#ifdef USE_CURSES
						mvwprintw(control,2,2,"Operate Success");
						#else
						printf("  Operate Success\n");
						#endif
					}
					else {
						#ifdef USE_CURSES
						mvwprintw(control,2,2,"Operate Failed");
						#else
						printf("  Operate Failed\n");
						#endif
					}
				}
				
				MmsValue_delete(ctlVal);
				ControlObjectClient_destroy(clientControl);
				
				#ifdef USE_CURSES
				wrefresh(control);
				#endif
				next_control = current_time + 2000;
			}
			else if( i == 'r' || i == 'R' ) {
				int nidx;
				MmsValue *result;
				
				#ifdef USE_CURSES
				wprintw(messages,"Forced Refresh:\n");
				#else
				printf("Forced Refresh:\n");
				#endif
				
				for( nidx=0; nidx<8; nidx++ ) {
					result = IedConnection_readObject(con,&error,dentrynames[nidx],IEC61850_FC_ST);
					if( result != NULL ) {
						#ifdef USE_CURSES
						wprintw(messages,"%s: \n",dentrynames[nidx]);
						#else
						printf("%s: \n",dentrynames[nidx]);
						#endif
						processEventsRecursive(dentrynames[nidx],result,1);
					}
					MmsValue_delete(result);
				}
				for( nidx=0; nidx<8; nidx++ ) {
					result = IedConnection_readObject(con,&error,aentrynames[nidx],IEC61850_FC_MX);
					if( result != NULL ) {
						#ifdef USE_CURSES
						wprintw(messages,"%s: \n",aentrynames[nidx]);
						#else
						printf("%s: \n",aentrynames[nidx]);
						#endif
						processMeasurementsRecursive(dentrynames[nidx],result,1);
					}
					MmsValue_delete(result);
				}

				#ifdef USE_CURSES
				wprintw(messages,"\n");
				wrefresh(messages);
				#endif
			}
			else if( i == 'c' || i == 'C' ) {
				#ifdef USE_CURSES
				wclear(messages);
				wrefresh(messages);
				#endif
			}
			//pthread_mutex_unlock(&lock);
		}
		
		if( !paused && current_time >= next_control ) {
			#ifdef USE_CURSES
			//pthread_mutex_lock(&lock);
			wclear(control);
			wrefresh(control);
			//pthread_mutex_unlock(&lock);
			#endif
		}

		#ifdef USE_CURSES
		if( !paused && current_time >= next_console ) {
			//pthread_mutex_lock(&lock);
			wclear(console);
			for( i=0; i<8; i++ ) {
				mvwprintw(console,i,0,"[%d]%s: ",i+1,dnames[i]);
				if( dv[i] == -1 ) {
					wattron(console,A_BLINK);
					wprintw(console,"?\n");
					wattroff(console,A_BLINK);
				}
				else if( dv[i] == 2 ) {
					wattron(console,A_BLINK);
					wprintw(console,"Err\n");
					wattroff(console,A_BLINK);
				}
				else if( dv[i] == 1 ) {
					wprintw(console,"[1] closed\n");
				}
				else {
					wattron(console,A_REVERSE);
					wprintw(console,"[0] open\n");
					wattroff(console,A_REVERSE);
				}
			}
			for( i=0; i<8; i++ ) {
				mvwprintw(console,8+i,0,"%s: ",anames[i]);
				if( av[i] == -1.0 ) {
					wattron(console,A_BLINK);
					wprintw(console,"?\n");
					wattroff(console,A_BLINK);
				} 
				else if( av[i] == 0.0 ) {
					wattron(console,A_REVERSE);
					wprintw(console,"%0.3f\n",av[i]);
					wattroff(console,A_REVERSE);
				}
				else {
					wprintw(console,"%0.3f\n",av[i]);
				}
			}
			wrefresh(console);
			//pthread_mutex_unlock(&lock);
			next_console = current_time + 1000;
		}
		#endif
		
		#ifdef USE_SIXEL
		if( !paused && current_time >= next_console ) {
			//pthread_mutex_lock(&lock);
			sixelDisplay();
			//pthread_mutex_unlock(&lock);
			next_console = current_time + 2000;
		}
		#endif
		usleep(250000);
	}
	
	IedConnection_close(con);
	IedConnection_destroy(con);
	
	#ifdef USE_CURSES
	delwin(console);
	delwin(messages);
	delwin(control);
	endwin();
	#else
	resetStdin();
	#endif
	return 0;
}


