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
#define __MODBUS_C__
#include "modbus.h"
#include "modbusvar.h"

#define MODBUSMSGLEN 1024
#define RXTIMEOUT 250

uint8_t modbus_address = 0;

static uint8_t req[MODBUSMSGLEN];
static uint16_t req_len;
static uint8_t res[MODBUSMSGLEN];
static uint16_t res_len;
static uint16_t calc_crc;
static uint16_t msg_idx;
static unsigned long last_recv;

static uint16_t offset;
static union {
  uint16_t count;
  uint16_t reg_value;
};
static uint16_t i;
static uint16_t analog_value;
static uint8_t digital_value;
static uint8_t byte_count;
static uint8_t bit_count;
static int success;

static const uint16_t CRC16TABLE[256] = {
  0x0000,0xC0C1,0xC181,0x0140,0xC301,0x03C0,0x0280,0xC241,
  0xC601,0x06C0,0x0780,0xC741,0x0500,0xC5C1,0xC481,0x0440,
  0xCC01,0x0CC0,0x0D80,0xCD41,0x0F00,0xCFC1,0xCE81,0x0E40,
  0x0A00,0xCAC1,0xCB81,0x0B40,0xC901,0x09C0,0x0880,0xC841,
  0xD801,0x18C0,0x1980,0xD941,0x1B00,0xDBC1,0xDA81,0x1A40,
  0x1E00,0xDEC1,0xDF81,0x1F40,0xDD01,0x1DC0,0x1C80,0xDC41,
  0x1400,0xD4C1,0xD581,0x1540,0xD701,0x17C0,0x1680,0xD641,
  0xD201,0x12C0,0x1380,0xD341,0x1100,0xD1C1,0xD081,0x1040,
  0xF001,0x30C0,0x3180,0xF141,0x3300,0xF3C1,0xF281,0x3240,
  0x3600,0xF6C1,0xF781,0x3740,0xF501,0x35C0,0x3480,0xF441,
  0x3C00,0xFCC1,0xFD81,0x3D40,0xFF01,0x3FC0,0x3E80,0xFE41,
  0xFA01,0x3AC0,0x3B80,0xFB41,0x3900,0xF9C1,0xF881,0x3840,
  0x2800,0xE8C1,0xE981,0x2940,0xEB01,0x2BC0,0x2A80,0xEA41,
  0xEE01,0x2EC0,0x2F80,0xEF41,0x2D00,0xEDC1,0xEC81,0x2C40,
  0xE401,0x24C0,0x2580,0xE541,0x2700,0xE7C1,0xE681,0x2640,
  0x2200,0xE2C1,0xE381,0x2340,0xE101,0x21C0,0x2080,0xE041,
  0xA001,0x60C0,0x6180,0xA141,0x6300,0xA3C1,0xA281,0x6240,
  0x6600,0xA6C1,0xA781,0x6740,0xA501,0x65C0,0x6480,0xA441,
  0x6C00,0xACC1,0xAD81,0x6D40,0xAF01,0x6FC0,0x6E80,0xAE41,
  0xAA01,0x6AC0,0x6B80,0xAB41,0x6900,0xA9C1,0xA881,0x6840,
  0x7800,0xB8C1,0xB981,0x7940,0xBB01,0x7BC0,0x7A80,0xBA41,
  0xBE01,0x7EC0,0x7F80,0xBF41,0x7D00,0xBDC1,0xBC81,0x7C40,
  0xB401,0x74C0,0x7580,0xB541,0x7700,0xB7C1,0xB681,0x7640,
  0x7200,0xB2C1,0xB381,0x7340,0xB101,0x71C0,0x7080,0xB041,
  0x5000,0x90C1,0x9181,0x5140,0x9301,0x53C0,0x5280,0x9241,
  0x9601,0x56C0,0x5780,0x9741,0x5500,0x95C1,0x9481,0x5440,
  0x9C01,0x5CC0,0x5D80,0x9D41,0x5F00,0x9FC1,0x9E81,0x5E40,
  0x5A00,0x9AC1,0x9B81,0x5B40,0x9901,0x59C0,0x5880,0x9841,
  0x8801,0x48C0,0x4980,0x8941,0x4B00,0x8BC1,0x8A81,0x4A40,
  0x4E00,0x8EC1,0x8F81,0x4F40,0x8D01,0x4DC0,0x4C80,0x8C41,
  0x4400,0x84C1,0x8581,0x4540,0x8701,0x47C0,0x4680,0x8641,
  0x8201,0x42C0,0x4380,0x8341,0x4100,0x81C1,0x8081,0x4040,
};

static void calcCrc(uint8_t* msg, uint16_t len) {
    calc_crc = 0xFFFF;
  for(msg_idx=0; msg_idx<len; msg_idx++) {
    calc_crc = (calc_crc>>8) ^ CRC16TABLE[(calc_crc^msg[msg_idx]) & 0xFF];
  }
}
   
static int validCrc() {
  calcCrc(req,req_len-2);
  if( (calc_crc&0xFF) == req[req_len-2] &&
      (calc_crc>>8)   == req[req_len-1] ) {
      return 1;
  }
  else {
    return 0;
  }
}

static int inputRequest() {
  int c = Serial2.read();
  if( c == -1 ) { return 0; }
  if( last_recv+RXTIMEOUT < millis() ) {
    //Last message has timeout
    req_len = 0;
  }
  last_recv = millis();
  req[req_len++] = (int8_t) c;
  if( req_len == MODBUSMSGLEN ) {
    
  }
  if( req_len > 1 ) {
    if( (req[1]  < 7 && req_len == 8) ||
        (req[1] == 7 && req_len == 4) ||
        (req[1] == 8 && req_len == 8) ) {
      if( validCrc() ) { return 1; }
      else { req_len = 0; }
    }
    else if( (req[1] == 15 || req[1] == 16) && req_len > 6 && req_len == (9 + req[6]) ) {
      if( validCrc() ) { return 1; }
      else { req_len = 0; }
    }
  }
  return 0;
}

void beginModbus() {
  req_len = 0;
  res_len = 0;
  last_recv = 0;
}

void resetModbus() {
  modbus_address = 0;
  beginModbus();
}

void processModbus() {
  while( Serial2.available() ) {
    if( inputRequest() ) {
      if( req[0] != 0 && req[0] != modbus_address ) {
        req_len = 0;
        continue;
      }
      res[0] = modbus_address;
      res[1] = req[1];
      offset = (req[2]<<8) | req[3];
      count = (req[4]<<8) | req[5]; //Count and/or reg_value;
      if( req[1] == 1 ) {
        bit_count = 0;
        res[2] = 0; //Byte Count
        for( i=0; i<count; i++ ) {
          if( bit_count == 0 || (bit_count % 8) == 0 ){
            bit_count = 0;
            res[2]++;
            if( 3+res[2] >= MODBUSMSGLEN ) {
              success = 0;
              res[2] = 4; //Error Code
              break;
            }
            res[2+res[2]] = 0;
          }
          success = getDO(offset+i,&digital_value);
          if( !success ) { 
            res[2] = 2; //Error Code
            break; 
          }
          res[2+res[2]] = res[2+res[2]] | (digital_value<<bit_count);
          bit_count++;
        }
        res_len = 3+res[2];
      }
      else if( req[1] == 2 ) {
        bit_count = 0;
        res[2] = 0; //Byte Count
        for( i=0; i<count; i++ ) {
          if( bit_count == 0 || (bit_count % 8) == 0 ){
            bit_count = 0;
            res[2]++;
            if( 3+res[2] >= MODBUSMSGLEN ) {
              success = 0;
              res[2] = 4; //Error Code
              break;
            }
            res[2+res[2]] = 0;
          }
          success = getDI(offset+i,&digital_value);
          if( !success ) { 
            res[2] = 2; //Error Code
            break; 
          }
          res[2+res[2]] = res[2+res[2]] | (digital_value<<bit_count);
          bit_count++;
        }
        res_len = 3+res[2];
      }
      else if( req[1] == 3 ) {
        res[2] = 0; //Byte Count
        for( i=0; i<count; i++ ) {
          success = getAO(offset+i,&analog_value);
          if( !success ) { 
            res[2] = 2; //Error Code
            break; 
          }
          res[3+res[2]] = (analog_value>>8);
          res[4+res[2]] = (analog_value&0xFF);
          res[2] = res[2] + 2;
          if( 3+res[2] >= MODBUSMSGLEN ) {
              success = 0;
              res[2] = 4; //Error Code
              break;
          }
        }
        res_len = 3+res[2];
      }
      else if( req[1] == 4 ) {
        res[2] = 0; //Byte Count
        for( i=0; i<count; i++ ) {
          success = getAI(offset+i,&analog_value);
          if( !success ) { 
            res[2] = 2; //Error Code
            break; 
          }
          res[3+res[2]] = (analog_value>>8);
          res[4+res[2]] = (analog_value&0xFF);
          res[2] = res[2] + 2;
          if( 3+res[2] >= MODBUSMSGLEN ) {
              success = 0;
              res[2] = 4; //Error Code
              break;
          }
        }
        res_len = 3+res[2]; 
      }
      else if( req[1] == 5 ) {
        res[2] = req[2];
        res[3] = req[3];
        res[4] = req[4];
        res[5] = req[5];
        res_len = 6;  
        if( reg_value ) {
          success = setDO(offset,CLOSE);
        }
        else {
          success = setDO(offset,OPEN);
        }
        if( !success ) {
          res[2] = 2; //Error Code
        }
      }
      else if( req[1] == 6 ) {
        res[2] = req[2];
        res[3] = req[3];
        res[4] = req[4];
        res[5] = req[5];
        res_len = 6;  
        success = setAO(offset,reg_value);
        if( !success ) {
          res[2] = 2; //Error Code
        }
      }
      else if( req[1] == 7 ) {
        res[2] = 0; //Exception Status
        res_len = 3;
        success = 1;
      }
      else if( req[1] == 8 ) {
        res[2] = req[2];
        res[3] = req[3];
        res[4] = req[4];
        res[5] = req[5];
        res_len = 6;
        success = 1;
      }
      else if( req[1] == 15 ) {
        byte_count = 0;
        bit_count = 0;
        for( i=0; i<count; i++ ) {
          if( req[7+byte_count] & (1<<bit_count) ) {
            success = setDO(offset+i,CLOSE);
          }
          else {
            success = setDO(offset+i,OPEN);
          }
          if( !success ) {
            res[2] = 2; //Error Code
            break;
          }
          bit_count++;
          if( bit_count % 8 == 0 ) {
            byte_count++;
            if( byte_count >= req[6] || (7+byte_count) >= MODBUSMSGLEN ) {
              success = 0;
              res[2] = 4;
              break;
            }
          }
        }
        res[2] = req[2];
        res[3] = req[3];
        res[4] = req[4];
        res[5] = req[5];
        res_len = 6;
      }
      else if( req[1] == 16 ) {
        byte_count = 0;
        for( i=0; i<count; i++ ) {
          analog_value = (req[7+byte_count] << 8) | req[8+byte_count];
          success = setAO(offset+i,analog_value);
          if( !success ) {
            res[2] = 2; //Error Code
            break;
          }
          byte_count = byte_count+2;
          if( byte_count > req[6] || (byte_count+8) >= MODBUSMSGLEN) {
            success = 0;
            res[2] = 1; //Error Code
          }
        }
        res[2] = req[2];
        res[3] = req[3];
        res[4] = req[4];
        res[5] = req[5];
        res_len = 6;
      }
      else {
        success = 0;
        res[2] = 1;
      }

      //Adjust the function code in the response if an error occured
      if( !success ) {
        res[1] = res[1]|0x80;
        res_len = 3;
      }
      //Send the response if request was not a broadcast
      if( req[0] ) {
        calcCrc(res,res_len);
        Serial2.write(res,res_len);
        Serial2.write((calc_crc&0xFF));
        Serial2.write((calc_crc>>8)&0xFF);
      }

      req_len = 0;
    }
  }
}
