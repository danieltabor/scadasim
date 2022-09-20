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
#ifndef __IEC61850_H__
#define __IEC61850_H__

#include <stdint.h>

#ifndef __IEC61850_C__

//Server Related
extern char iec61850_serv_name[256];
extern uint16_t iec61850_serv_port;

//GOOSE (Digital) Related
extern char iec61850_goose_digital_eth[256];
extern uint16_t iec61850_goose_digital_appid;
extern char iec61850_goose_digital_dst[6];

//GOOSE (Analog) Related
extern char iec61850_goose_analog_eth[256];
extern uint16_t iec61850_goose_analog_appid;
extern char iec61850_goose_analog_dst[6];


//ICD Related
extern char iec61850_icd_path[256];
extern char iec61850_scd_path[256];
#endif //__IEC61850_C__

void iec61850Begin();
void iec61850Reset();
void iec61850Update();

void iec61850Serv();
void iec61850ServReset();

void iec61850GooseDigital();
void iec61850GooseDigitalReset();
void iec61850GooseAnalog();
void iec61850GooseAnalogReset();

void iec61850ExportIcd();
void iec61850ExportScd();

#endif //__IEC61850_H__
