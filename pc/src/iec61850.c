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
#define __IEC61850_C__
#include "iec61850.h"
#include <iec61850_server.h>
#include <mms_value.h>
#include <goose_publisher.h>

#include "cli.h"
#include "var.h"
#include "display.h"

#include <stdio.h>

#define IEDNAME "ScadaSim"
#define VENDOR "GTRI"
#define SWREV "1.0"
#define DESCR "BASIC SCADA Simulator"

char iec61850_serv_name[256];
uint16_t iec61850_serv_port;

char iec61850_goose_digital_eth[256];
uint16_t iec61850_goose_digital_appid;
char iec61850_goose_digital_dst[6];

char iec61850_goose_analog_eth[256];
uint16_t iec61850_goose_analog_appid;
char iec61850_goose_analog_dst[6];

char iec61850_icd_path[256];
char iec61850_scd_path[256];

static char* iec61850_serv_name_backup;
static IedServer iedServer;
static IedModel *iedModel;
static LogicalDevice *dev;
static LogicalNode *ggio;

static GoosePublisher iedEventsPublisher;
static GoosePublisher iedMeasurementsPublisher;
static LinkedList gooseEvents;
static LinkedList gooseMeasurements;

static LogicalNode* modelNode(char* name, DataObject** lastObj) {
	LogicalNode* node = LogicalNode_create(name,dev);
	DataObject* obj;
	
	obj = DataObject_create("Mod",(ModelNode*)node,0);
	DataAttribute_create("stVal",(ModelNode*)obj,IEC61850_ENUMERATED,IEC61850_FC_ST,TRG_OPT_DATA_CHANGED,0,0);
	DataAttribute_create("q",(ModelNode*)obj,IEC61850_QUALITY,IEC61850_FC_ST,TRG_OPT_DATA_CHANGED,0,0);
	DataAttribute_create("t",(ModelNode*)obj,IEC61850_TIMESTAMP,IEC61850_FC_ST,0,0,0);
	DataAttribute_create("ctlModel",(ModelNode*)obj,IEC61850_ENUMERATED,IEC61850_FC_ST,0,0,0);
	
	obj = DataObject_create("Beh",(ModelNode*)node,0);
	DataAttribute_create("stVal",(ModelNode*)obj,IEC61850_ENUMERATED,IEC61850_FC_ST,TRG_OPT_DATA_CHANGED,0,0);
	DataAttribute_create("q",(ModelNode*)obj,IEC61850_QUALITY,IEC61850_FC_ST,TRG_OPT_DATA_CHANGED,0,0);
	DataAttribute_create("t",(ModelNode*)obj,IEC61850_TIMESTAMP ,IEC61850_FC_ST,0,0,0);
	
	obj = DataObject_create("Health",(ModelNode*)node,0);
	DataAttribute_create("stVal",(ModelNode*)obj,IEC61850_ENUMERATED,IEC61850_FC_ST,TRG_OPT_DATA_CHANGED,0,0);
	DataAttribute_create("q",(ModelNode*)obj,IEC61850_QUALITY,IEC61850_FC_ST,TRG_OPT_DATA_CHANGED,0,0);
	DataAttribute_create("t",(ModelNode*)obj,IEC61850_TIMESTAMP,IEC61850_FC_ST,0,0,0);

	obj = DataObject_create("NamPlt",(ModelNode*)node,0);
	DataAttribute_create("vendor",(ModelNode*)obj,IEC61850_VISIBLE_STRING_255,IEC61850_FC_DC,0,0,0);
	DataAttribute_create("swRev",(ModelNode*)obj,IEC61850_VISIBLE_STRING_255,IEC61850_FC_DC,0,0,0);
	DataAttribute_create("d",(ModelNode*)obj,IEC61850_VISIBLE_STRING_255,IEC61850_FC_DC,0,0,0);

	if( lastObj != 0 ) {
		*lastObj = obj;
	}
	
	return node;
}


static LogicalNode* modelLLN0() {
	unsigned int i;
	DataObject* obj;
	LogicalNode* node;
	DataSet* set;
	ReportControlBlock* rcb;
	char variable[32];
	node = modelNode("LLN0",&obj);
	uint8_t digitalPoints = 0;
	uint8_t analogPoints = 0;
	
	DataAttribute_create("configRev",(ModelNode*)obj,IEC61850_VISIBLE_STRING_255,IEC61850_FC_DC,0,0,0);
	DataAttribute_create("ldNs",(ModelNode*)obj,IEC61850_VISIBLE_STRING_255,IEC61850_FC_EX,0,0,0);
	
	//Check to see if there are digital and/or analog points to export to IEC61850
	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].pnttype == PNT_DO || 
		    vars[i].pnttype == PNT_DI ) {
			digitalPoints = 1;
		}
		else if( vars[i].pnttype == PNT_AO || vars[i].pnttype == PNT_AO_SCALED ||
		    vars[i].pnttype == PNT_AI || vars[i].pnttype == PNT_AI_SCALED ) {
			analogPoints = 1;
		} 
	}
	
	//Create Dataset for DO/DI
	if( digitalPoints ) {
		set = DataSet_create("dsEvents", node);
		for( i=0; i<VARSMAX; i++ ) {
			if( vars[i].pnttype == PNT_DO ) {
				snprintf(variable,32,"GGIO1$ST$SPCSO%d$stVal",vars[i].pntaddr);
				DataSetEntry_create(set,variable,-1,0);
			}
			if( vars[i].pnttype == PNT_DI ) {
				snprintf(variable,32,"GGIO1$ST$Ind%d$stVal",vars[i].pntaddr);
				DataSetEntry_create(set,variable,-1,0);
			}
		}
	}
	
	//Create Dataset of AO/AI Measurements
	if( analogPoints ) {
		set = DataSet_create("dsMeasurements", node);
		for( i=0; i<VARSMAX; i++ ) {
			if( vars[i].pnttype == PNT_AO || vars[i].pnttype == PNT_AO_SCALED ) {
				snprintf(variable,32,"GGIO1$MX$AnOut%d$mag$f",vars[i].pntaddr);
				DataSetEntry_create(set,variable,-1,0);
			}
			else if( vars[i].pnttype == PNT_AI || vars[i].pnttype == PNT_AI_SCALED ) {
				snprintf(variable,32,"GGIO1$MX$AnIn%d$mag$f",vars[i].pntaddr);
				DataSetEntry_create(set,variable,-1,0);
			}
		}
	}
	
	//Create ReportControlBlock for Events
	if( digitalPoints ) {
		rcb = ReportControlBlock_create("brcbEvents",node,
			"brEvents",true,"dsEvents",1,
			TRG_OPT_DATA_CHANGED|TRG_OPT_DATA_UPDATE|TRG_OPT_GI|TRG_OPT_INTEGRITY,
			RPT_OPT_SEQ_NUM|RPT_OPT_TIME_STAMP|RPT_OPT_DATA_SET|RPT_OPT_REASON_FOR_INCLUSION|RPT_OPT_CONF_REV,
			TICKDELAY/10,DISPLAYDELAY);
		rcb = ReportControlBlock_create("urcbEvents",node,
			"rpEvents",false,"dsEvents",1,
			TRG_OPT_DATA_CHANGED|TRG_OPT_DATA_UPDATE|TRG_OPT_GI|TRG_OPT_INTEGRITY,
			RPT_OPT_SEQ_NUM|RPT_OPT_TIME_STAMP|RPT_OPT_DATA_SET|RPT_OPT_REASON_FOR_INCLUSION|RPT_OPT_CONF_REV,
			TICKDELAY/10,DISPLAYDELAY);
		//DataAttribute_create("RptID",(ModelNode*)rcb,IEC61850_INT32,IEC61850_FC_BR,0,0,0);
	}
	
	//CreateReportControlBlock for Measurements
	if( analogPoints ) {
		rcb = ReportControlBlock_create("brcbMeasurements",node,
			"brMeasurements",true,"dsMeasurements",1,
			TRG_OPT_DATA_CHANGED|TRG_OPT_DATA_UPDATE|TRG_OPT_GI|TRG_OPT_INTEGRITY,
			RPT_OPT_SEQ_NUM|RPT_OPT_TIME_STAMP|RPT_OPT_DATA_SET|RPT_OPT_REASON_FOR_INCLUSION|RPT_OPT_CONF_REV,
			TICKDELAY/10,DISPLAYDELAY);	
		rcb = ReportControlBlock_create("urcbMeasurements",node,
			"rpMeasurements",false,"dsMeasurements",1,
			TRG_OPT_DATA_CHANGED|TRG_OPT_DATA_UPDATE|TRG_OPT_GI|TRG_OPT_INTEGRITY,
			RPT_OPT_SEQ_NUM|RPT_OPT_TIME_STAMP|RPT_OPT_DATA_SET|RPT_OPT_REASON_FOR_INCLUSION|RPT_OPT_CONF_REV,
			TICKDELAY/10,DISPLAYDELAY);	
		//DataAttribute_create("RptID",(ModelNode*)rcb,IEC61850_INT32,IEC61850_FC_BR,0,0,0);
	}
	
	
}

static void modelLLN0NamePlt() {
	DataObject* lln0 = (DataObject*)(iedModel->firstChild->firstChild);
	DataObject* namplt = (DataObject*)(lln0->firstChild->sibling->sibling->sibling);
	DataAttribute* vendor = (DataAttribute*)(namplt->firstChild);
	DataAttribute* swrev = (DataAttribute*)(vendor->sibling);
	DataAttribute* descr = (DataAttribute*)(swrev->sibling);
	
	IedServer_updateVisibleStringAttributeValue(iedServer,vendor,VENDOR);
	IedServer_updateVisibleStringAttributeValue(iedServer,swrev,SWREV);
	IedServer_updateVisibleStringAttributeValue(iedServer,descr,DESCR);

	//DataAttribute* rptId = (DataAttribute*)(namplt->sibling->firstChild);
	//printf("Meaurements: %s\n",namplt->sibling->name);fflush(0);
	//printf("rptId: %s\n",rptId->name);fflush(0);
	//IedServer_updateInt32AttributeValue(iedServer,rptId,25);
}

static void modelLPHD1() {
	LogicalNode* node = LogicalNode_create("LPHD1",dev);
	DataObject* obj;
	
	obj = DataObject_create("PhyNam",(ModelNode*)node,0);
	DataAttribute_create("vendor",(ModelNode*)obj,IEC61850_VISIBLE_STRING_255,IEC61850_FC_DC,0,0,0);

	obj = DataObject_create("PhyHealth",(ModelNode*)node,0);
	DataAttribute_create("stVal",(ModelNode*)obj,IEC61850_ENUMERATED,IEC61850_FC_ST,TRG_OPT_DATA_CHANGED,0,0);
	DataAttribute_create("q",(ModelNode*)obj,IEC61850_QUALITY,IEC61850_FC_ST,TRG_OPT_DATA_CHANGED,0,0);
	DataAttribute_create("t",(ModelNode*)obj,IEC61850_TIMESTAMP,IEC61850_FC_ST,0,0,0);

	obj = DataObject_create("Proxy",(ModelNode*)node,0);
	DataAttribute_create("stVal",(ModelNode*)obj,IEC61850_BOOLEAN,IEC61850_FC_ST,TRG_OPT_DATA_CHANGED,0,0);
	DataAttribute_create("q",(ModelNode*)obj,IEC61850_QUALITY,IEC61850_FC_ST,TRG_OPT_DATA_CHANGED,0,0);
	DataAttribute_create("t",(ModelNode*)obj,IEC61850_TIMESTAMP,IEC61850_FC_ST,0,0,0);
}

static ControlHandlerResult 
modelControl(ControlAction action, void* parameter, MmsValue* value, bool test) {
	var_t* v = (var_t*)parameter;
	
	if (test) {
        return CONTROL_RESULT_FAILED;
	}

    if (MmsValue_getType(value) != MMS_BOOLEAN) {
        return CONTROL_RESULT_FAILED;
	}

	if( MmsValue_getBoolean(value) ) {
		MAKE_ONE(v->value);
		v->expr = 0;
	}
	else {
		MAKE_ZERO(v->value);
		v->expr = 0;
	}
		
    return CONTROL_RESULT_OK;
}

static void modelDOCallbacks() {
	unsigned int i;
	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].pnttype == PNT_DO ) {
			IedServer_updateCtlModel(iedServer,vars[i].iec61850_ctrl,CONTROL_MODEL_DIRECT_NORMAL);
			IedServer_setControlHandler(iedServer, (DataObject*)(vars[i].iec61850_ctrl),
			(ControlHandler) modelControl,
			&(vars[i]));
		}
	}
}

static void modelDO(var_t* v) {
	DataObject* obj;
	DataAttribute* oper;
	DataAttribute* origin;
	char name[16];
	snprintf(name,16,"SPCSO%d",v->pntaddr);
	obj = DataObject_create(name,(ModelNode*)ggio,0);
	
	v->iec61850_value = 
	DataAttribute_create("stVal",(ModelNode*)obj,IEC61850_BOOLEAN,IEC61850_FC_ST,TRG_OPT_DATA_CHANGED,0,0);
	DataAttribute_create("q",(ModelNode*)obj,IEC61850_QUALITY,IEC61850_FC_ST,TRG_OPT_DATA_CHANGED,0,0);
	DataAttribute_create("ctlModel",(ModelNode*)obj,IEC61850_ENUMERATED,IEC61850_FC_CF,0,0,0);
	v->iec61850_timestamp =
	DataAttribute_create("t",(ModelNode*)obj,IEC61850_TIMESTAMP,IEC61850_FC_ST,0,0,0);
	
	oper = DataAttribute_create("Oper",(ModelNode*)obj,IEC61850_CONSTRUCTED,IEC61850_FC_CO,0,0,0);
	DataAttribute_create("ctlVal",(ModelNode*)oper,IEC61850_BOOLEAN,IEC61850_FC_CO,0,0,0);
	origin = DataAttribute_create("origin",(ModelNode*)oper,IEC61850_CONSTRUCTED,IEC61850_FC_CO,0,0,0);
	DataAttribute_create("orCat",(ModelNode*)origin,IEC61850_ENUMERATED,IEC61850_FC_CO,0,0,0);
	DataAttribute_create("orIndent",(ModelNode*)origin,IEC61850_OCTET_STRING_64,IEC61850_FC_CO,0,0,0);
	DataAttribute_create("ctlNum",(ModelNode*)oper,IEC61850_INT8U,IEC61850_FC_ST,0,0,0);	
	DataAttribute_create("T",(ModelNode*)oper,IEC61850_TIMESTAMP,IEC61850_FC_CO,0,0,0);
	DataAttribute_create("Test",(ModelNode*)oper,IEC61850_BOOLEAN,IEC61850_FC_CO,0,0,0);
	DataAttribute_create("Check",(ModelNode*)oper,IEC61850_CHECK,IEC61850_FC_CO,0,0,0);
	
	v->iec61850_ctrl = obj;
}


static void modelDI(var_t* v) {
	DataObject* obj;
	char name[16];
	snprintf(name,16,"Ind%d",v->pntaddr);
	obj = DataObject_create(name,(ModelNode*)ggio,0);
	v->iec61850_value =
	DataAttribute_create("stVal",(ModelNode*)obj,IEC61850_BOOLEAN,IEC61850_FC_ST,TRG_OPT_DATA_CHANGED,0,0);
	DataAttribute_create("q",(ModelNode*)obj,IEC61850_QUALITY   ,IEC61850_FC_ST,TRG_OPT_DATA_CHANGED,0,0);
	v->iec61850_timestamp = 
	DataAttribute_create("t",(ModelNode*)obj,IEC61850_TIMESTAMP ,IEC61850_FC_ST,0,0,0);
}


static void modelAO(var_t* v) {
	DataObject* obj;
	DataAttribute* mag;
	DataAttribute* oper;
	char name[16];
	snprintf(name,16,"AnOut%d",v->pntaddr);
	obj = DataObject_create(v->name,(ModelNode*)ggio,0);
	mag = DataAttribute_create("mag",(ModelNode*)obj,IEC61850_CONSTRUCTED,IEC61850_FC_MX,TRG_OPT_DATA_CHANGED,0,0);
	v->iec61850_value =
	DataAttribute_create("f",(ModelNode*)mag,IEC61850_FLOAT32,IEC61850_FC_MX,TRG_OPT_DATA_CHANGED,0,0);
	DataAttribute_create("q",(ModelNode*)obj,IEC61850_QUALITY,IEC61850_FC_MX,TRG_OPT_DATA_CHANGED,0,0);
	v->iec61850_timestamp = 
	DataAttribute_create("t",(ModelNode*)obj,IEC61850_TIMESTAMP ,IEC61850_FC_MX,0,0,0);
	oper = DataAttribute_create("Oper",(ModelNode*)obj,IEC61850_CONSTRUCTED,IEC61850_FC_SP,0,0,0);
	DataAttribute_create("f",(ModelNode*)oper,IEC61850_FLOAT32,IEC61850_FC_SP,0,0,0);
}


static void modelAI(var_t* v) {
	DataObject* obj;
	DataAttribute* mag;
	char name[16];
	snprintf(name,16,"AnIn%d",v->pntaddr);
	obj = DataObject_create(name,(ModelNode*)ggio,0);
	mag = DataAttribute_create("mag",(ModelNode*)obj,IEC61850_CONSTRUCTED,IEC61850_FC_MX,TRG_OPT_DATA_CHANGED,0,0);
	v->iec61850_value =
	DataAttribute_create("f",(ModelNode*)mag,IEC61850_FLOAT32,IEC61850_FC_MX,TRG_OPT_DATA_CHANGED,0,0);
	DataAttribute_create("q",(ModelNode*)obj,IEC61850_QUALITY,IEC61850_FC_MX,TRG_OPT_DATA_CHANGED,0,0);
	v->iec61850_timestamp =
	DataAttribute_create("t",(ModelNode*)obj,IEC61850_TIMESTAMP ,IEC61850_FC_MX,0,0,0);
}


static LogicalNode* modelGGIO1() {
	unsigned int i;
	
	ggio = modelNode("GGIO1",0);

	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].pnttype == PNT_DO ) {
			modelDO(&vars[i]);
		}
		else if( vars[i].pnttype == PNT_DI ) {
			modelDI(&vars[i]);
		}
		else if( vars[i].pnttype == PNT_AO || vars[i].pnttype == PNT_AO_SCALED ) {
			modelAO(&vars[i]);
		}
		else if( vars[i].pnttype == PNT_AI  || vars[i].pnttype == PNT_AI_SCALED ) {
			modelAI(&vars[i]);
		}
	}
}


static void modelUpdate(int force) {
	unsigned int i;
	bool b;
	float v,f;
	uint64_t timestamp;
	
	if( iedServer == 0 ) {
		return;
	}
	
	IedServer_lockDataModel(iedServer);
	
	timestamp = Hal_getTimeInMs();
	Timestamp iecTimestamp;
	Timestamp_clearFlags(&iecTimestamp);
	Timestamp_setTimeInMilliseconds(&iecTimestamp, timestamp);
	Timestamp_setLeapSecondKnown(&iecTimestamp, true);
	
	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].pnttype == PNT_DO || vars[i].pnttype == PNT_DI ) {
			if( (vars[i].value.type == VAL_INT && vars[i].value.i != 0 ) ||
				(vars[i].value.type == VAL_FLOAT && vars[i].value.f != 0.0 ) ) {
					b = true;
			}
			else {
				b = false;
			}
			if( force || IedServer_getBooleanAttributeValue(iedServer,(DataAttribute*)vars[i].iec61850_value) != b ) {
				IedServer_updateBooleanAttributeValue(iedServer,(DataAttribute*)vars[i].iec61850_value,b);
				IedServer_updateTimestampAttributeValue(iedServer,(DataAttribute*)vars[i].iec61850_timestamp,&iecTimestamp);
			}
		}
		else if( vars[i].pnttype == PNT_AO || vars[i].pnttype == PNT_AI ) {
			if( vars[i].value.type == VAL_INT ) {
				f = (float)vars[i].value.i;
			}
			else {
				f = vars[i].value.f;
			}
			if( force || IedServer_getFloatAttributeValue(iedServer,(DataAttribute*)vars[i].iec61850_value) != f ) {
				IedServer_updateFloatAttributeValue(iedServer,(DataAttribute*)vars[i].iec61850_value,f);
				IedServer_updateTimestampAttributeValue(iedServer,(DataAttribute*)vars[i].iec61850_timestamp,&iecTimestamp);
			}
		}
		else if( vars[i].pnttype == PNT_AO_SCALED || vars[i].pnttype == PNT_AI_SCALED ) {
			if( vars[i].value.type == VAL_INT ) {
				f = ((float)vars[i].value.i-vars[i].pntmin) / (vars[i].pntmax-vars[i].pntmin);
			}
			else {
				f = (vars[i].value.f-vars[i].pntmin) / (vars[i].pntmax-vars[i].pntmin);
			}
			if( f < vars[i].pntmin ) {
				f = 0.0;
			}
			else if( f > vars[i].pntmax ) {
				f = 1.0;
			}
			if( force || IedServer_getFloatAttributeValue(iedServer,(DataAttribute*)vars[i].iec61850_value) != f ) {
				IedServer_updateFloatAttributeValue(iedServer,(DataAttribute*)vars[i].iec61850_value,f);
				IedServer_updateTimestampAttributeValue(iedServer,(DataAttribute*)vars[i].iec61850_timestamp,&iecTimestamp);
			}
		}
	}
	
	IedServer_unlockDataModel(iedServer);
}


static void gooseDigitalUpdate( int create ) {
	unsigned int i;
	int nidx = 0;
	bool b;
	int pub = create;
	LinkedList node;
	MmsValue* value;
	
	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].pnttype == PNT_DO || vars[i].pnttype == PNT_DI ) {
			if( (vars[i].value.type == VAL_INT && vars[i].value.i != 0 ) ||
				(vars[i].value.type == VAL_FLOAT && vars[i].value.f != 0.0 ) ) {
				b = true;
			}
			else {
				b = false;
			}
			if( create ) {
				LinkedList_add(gooseEvents, MmsValue_newBoolean(b));
			} else {
				node = LinkedList_get(gooseEvents,nidx++);
				if( node ) {
					value = (MmsValue*)LinkedList_getData(node);
					if( value ) {
						if( MmsValue_getBoolean(value) != b ) {
							pub = 1;
							MmsValue_setBoolean(value,b);
						}
					}
				}
			}
		}
	}

	if( pub ) {
		GoosePublisher_publish(iedEventsPublisher, gooseEvents);
	}
}

static void gooseAnalogUpdate(int create) {
	unsigned int i;
	int nidx = 0;
	float f;
	int pub = create;
	LinkedList node;
	MmsValue* value;
	
	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].pnttype == PNT_AO || vars[i].pnttype == PNT_AI ) {
			if( vars[i].value.type == VAL_INT ) {
				f = (float)vars[i].value.i;
			}
			else {
				f = vars[i].value.f;
			}
			if( create ) {
				LinkedList_add(gooseMeasurements,MmsValue_newFloat(f));
			} else {
				node = LinkedList_get(gooseMeasurements,nidx++);
				if( node ) {
					if( value ) {
						value = (MmsValue*)LinkedList_getData(node);
						if( MmsValue_toFloat(value) != f ) {
							pub = 1;
							MmsValue_setFloat(value,f);
						}
					}
				}			
			}
		}
		else if( vars[i].pnttype == PNT_AO_SCALED || vars[i].pnttype == PNT_AI_SCALED ) {
			if( vars[i].value.type == VAL_INT ) {
				f = ((float)vars[i].value.i-vars[i].pntmin) / (vars[i].pntmax-vars[i].pntmin);
			}
			else {
				f = (vars[i].value.f-vars[i].pntmin) / (vars[i].pntmax-vars[i].pntmin);
			}
			if( f < vars[i].pntmin ) {
				f = 0.0;
			}
			else if( f > vars[i].pntmax ) {
				f = 1.0;
			}
			if( create ) {
				LinkedList_add(gooseMeasurements, MmsValue_newFloat(f));
			} else {
				node = LinkedList_get(gooseMeasurements,nidx++);
				if( node ) {
					value = (MmsValue*)LinkedList_getData(node);
					if( value ) {
						if( MmsValue_toFloat(value) != f ) {
							pub = 1;
							MmsValue_setFloat(value,f);
						}
					}
				}
			}				
		}
	}

	if( pub ) {
		GoosePublisher_publish(iedMeasurementsPublisher, gooseMeasurements);
	}
}


static void iec61850ServBegin() {
	iedServer = 0;
	iec61850_serv_name[0] = 0;
	iec61850_serv_port = 61850;
}


static void iec61850GooseDigitalBegin() {
	iedEventsPublisher = 0;
	gooseEvents = 0;
	iec61850_goose_digital_eth[0] = 0;
	iec61850_goose_digital_appid = 0;
	iec61850_goose_digital_dst[0] = 0xFF;
	iec61850_goose_digital_dst[1] = 0xFF;
	iec61850_goose_digital_dst[2] = 0xFF;
	iec61850_goose_digital_dst[3] = 0xFF;
	iec61850_goose_digital_dst[4] = 0xFF;
	iec61850_goose_digital_dst[5] = 0xFF;
}

static void iec61850GooseAnalogBegin() {
	iedMeasurementsPublisher = 0;
	gooseMeasurements = 0;
	iec61850_goose_analog_eth[0] = 0;
	iec61850_goose_analog_appid = 0;
	iec61850_goose_analog_dst[0] = 0xFF;
	iec61850_goose_analog_dst[1] = 0xFF;
	iec61850_goose_analog_dst[2] = 0xFF;
	iec61850_goose_analog_dst[3] = 0xFF;
	iec61850_goose_analog_dst[4] = 0xFF;
	iec61850_goose_analog_dst[5] = 0xFF;
}

void iec61850Begin() {
	iec61850ServBegin();
	iec61850GooseDigitalBegin();
	iec61850GooseAnalogBegin();
	iec61850_icd_path[0] = 0;
}


void iec61850Serv() {
	unsigned int i;
	
    // Create configuration based on library example
    IedServerConfig config = IedServerConfig_create();
    IedServerConfig_setReportBufferSize(config, 200000);
    IedServerConfig_setEdition(config, IEC_61850_EDITION_2);
    IedServerConfig_setFileServiceBasePath(config, "./vmd-filestore/");
    IedServerConfig_enableFileService(config, false);
    IedServerConfig_enableDynamicDataSetService(config, true);
    IedServerConfig_enableLogService(config, true);
    IedServerConfig_setMaxMmsConnections(config, 2);

	// Set-up iedModel with basic info
	iedModel = IedModel_create("");
	iec61850_serv_name_backup = iedModel->name;
	iedModel->name = iec61850_serv_name;
	dev = LogicalDevice_create("GenericIO",iedModel);
	modelLLN0();
	modelLPHD1();
	modelGGIO1();
	
    // Create a new IEC 61850 server instance
    iedServer = IedServer_createWithConfig(iedModel, NULL, config);
	IedServerConfig_destroy(config);
	IedServer_setServerIdentity(iedServer, "Simulation", "ScadaSim", "");
	
	modelUpdate(1);
	modelLLN0NamePlt();
	modelDOCallbacks();
	
	IedServer_start(iedServer, iec61850_serv_port);
}

void iec61850GooseDigital() {
	char ref[296];
	
	CommParameters gooseCommParameters;
    gooseCommParameters.appId = iec61850_goose_digital_appid;
    gooseCommParameters.dstAddress[0] = iec61850_goose_digital_dst[0];
    gooseCommParameters.dstAddress[1] = iec61850_goose_digital_dst[1];
    gooseCommParameters.dstAddress[2] = iec61850_goose_digital_dst[2];
    gooseCommParameters.dstAddress[3] = iec61850_goose_digital_dst[3];
    gooseCommParameters.dstAddress[4] = iec61850_goose_digital_dst[4];
    gooseCommParameters.dstAddress[5] = iec61850_goose_digital_dst[5];
    gooseCommParameters.vlanId = 0;
    gooseCommParameters.vlanPriority = 4;
	
	iedEventsPublisher = GoosePublisher_create(&gooseCommParameters, iec61850_goose_digital_eth);
	if( iedEventsPublisher == 0 ) { iec61850GooseDigitalReset(); return; }
	snprintf(ref,sizeof(ref),"%sGenericIO/LLN0$GO$gcbEvents",iec61850_serv_name);
	GoosePublisher_setGoCbRef(iedEventsPublisher,ref);
	snprintf(ref,sizeof(ref),"%sGenericIO/LLN0$dsEvents",iec61850_serv_name);
	GoosePublisher_setDataSetRef(iedEventsPublisher,ref);
	GoosePublisher_setConfRev(iedEventsPublisher, 1);
	GoosePublisher_setTimeAllowedToLive(iedEventsPublisher, 500);
	
	gooseEvents = LinkedList_create();
	if( gooseEvents == 0 ) {  iec61850GooseDigitalReset(); return; }
	gooseDigitalUpdate(1);
}

void iec61850GooseAnalog() {
	char ref[296];
	
	CommParameters gooseCommParameters;
    gooseCommParameters.appId = iec61850_goose_analog_appid;;
    gooseCommParameters.dstAddress[0] = iec61850_goose_analog_dst[0];
    gooseCommParameters.dstAddress[1] = iec61850_goose_analog_dst[1];
    gooseCommParameters.dstAddress[2] = iec61850_goose_analog_dst[2];
    gooseCommParameters.dstAddress[3] = iec61850_goose_analog_dst[3];
    gooseCommParameters.dstAddress[4] = iec61850_goose_analog_dst[4];
    gooseCommParameters.dstAddress[5] = iec61850_goose_analog_dst[5];
    gooseCommParameters.vlanId = 0;
    gooseCommParameters.vlanPriority = 4;
	
	iedMeasurementsPublisher = GoosePublisher_create(&gooseCommParameters, iec61850_goose_analog_eth);
	if( iedMeasurementsPublisher == 0 ) { iec61850GooseAnalogReset(); return; }
	snprintf(ref,sizeof(ref),"%sGenericIO/LLN0$GO$gcbMeasurements",iec61850_serv_name);
	GoosePublisher_setGoCbRef(iedMeasurementsPublisher,ref);
	snprintf(ref,sizeof(ref),"%sGenericIO/LLN0$dsMeasurements",iec61850_serv_name);
	GoosePublisher_setDataSetRef(iedMeasurementsPublisher,ref);
	GoosePublisher_setConfRev(iedMeasurementsPublisher, 1);
	GoosePublisher_setTimeAllowedToLive(iedMeasurementsPublisher, 500);
	
	gooseMeasurements = LinkedList_create();
	if( gooseMeasurements == 0 ) {  iec61850GooseAnalogReset(); return; }
	gooseAnalogUpdate(1);
}

void iec61850ServReset() {
	if( iedServer != 0 ) {
		unsigned int i;
		IedServer_stop(iedServer);
		
		for( i=0; i<VARSMAX; i++ ) {
			vars[i].iec61850_value = 0;
			vars[i].iec61850_timestamp = 0;
		}
		
		IedServer_destroy(iedServer);
		iedModel->name = iec61850_serv_name_backup;
		IedModel_destroy(iedModel);
	}
	iec61850ServBegin();
}

void iec61850GooseDigitalReset() {
	if( iedEventsPublisher != 0 ) {
		GoosePublisher_destroy(iedEventsPublisher);
		LinkedList_destroyDeep(gooseEvents, (LinkedListValueDeleteFunction) MmsValue_delete);
	}
	iec61850GooseDigitalBegin();
}

void iec61850GooseAnalogReset() {
	if( iedMeasurementsPublisher != 0 ) {
		GoosePublisher_destroy(iedMeasurementsPublisher);
		LinkedList_destroyDeep(gooseMeasurements, (LinkedListValueDeleteFunction) MmsValue_delete);
	}
	iec61850GooseAnalogBegin();
}

void iec61850Reset() {
	iec61850ServReset();
	iec61850GooseDigitalReset();
	iec61850GooseAnalogReset();
}

void iec61850Update() {
	if( newVars ) {
		if( iedServer!= 0 ) {
			modelUpdate(0);
		}
		if( iedEventsPublisher != 0 ) {
			gooseDigitalUpdate(0);
		}
		if( iedMeasurementsPublisher != 0 ) {
			gooseAnalogUpdate(0);
		}
	}
}

///////////////////////////
// ICD/XML Export
///////////////////////////

static char XML_TEMPLATE_1[] = \
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" \
"<SCL xmlns=\"http://www.iec.ch/61850/2003/SCL\">\n" \
"  <Header id=\"\" nameStructure=\"IEDName\">\n" \
"  </Header>\n";

static char XML_TEMPLATE_COMMUNICATION_1[] = \
"  <Communication>\n"\
"    <SubNetwork name=\"None\" type=\"8-MMS\">\n" \
"      <ConnectedAP apName=\"AP1\" iedName=\"%s\">\n"\
"        <Address>\n"\
"          <P type=\"IP\">0.0.0.0</P>\n"\
"          <P type=\"IP-SUBNET\">0.0.0.0</P>\n"\
"          <P type=\"IP-GATEWAY\">0.0.0.0</P>\n"\
"        </Address>\n";

static char XML_TEMPLATE_COMMUNICATION_GSE[] = \
"        <GSE cbName=\"%s\" ldInst=\"GenericIO\">\n"\
"          <Address>\n"\
"            <P type=\"MAC-Address\">%02X:%02X:%02X:%02X:%02X:%02X</P>\n"\
"            <P type=\"APPID\">%d</P>\n"\
"            <P type=\"VLAN-ID\">000</P>\n"\
"            <P type=\"VLAN-PRIORITY\">4</P>\n"\
"          </Address>\n"\
"          <MinTime multiplier=\"m\" unit=\"s\">250</MinTime>\n"\
"          <MaxTime multiplier=\"m\" unit=\"s\">2000</MaxTime>\n"\
"        </GSE>\n";

static char XML_TEMPLATE_COMMUNICATION_2[] = \
"      </ConnectedAP>\n" \
"    </SubNetwork>\n" \
"  </Communication>\n";

static char XML_TEMPLATE_2[] = \
"  <IED name=\"%s\">\n" \
"    <Services>\n" \
"      <DynAssociation />\n" \
"      <GetDirectory />\n" \
"      <GetDataObjectDefinition />\n" \
"      <GetDataSetValue />\n" \
"      <DataSetDirectory />\n" \
"      <ReadWrite />\n" \
"      <GetCBValues />\n" \
"      <ConfLNs fixPrefix=\"true\" fixLnInst=\"true\" />\n" \
"      <GOOSE max=\"5\" />\n" \
"      <GSSE max=\"5\" />\n" \
"      <FileHandling />\n" \
"      <GSEDir />\n" \
"      <TimerActivatedControl />\n" \
"    </Services>\n" \
"    <AccessPoint name=\"AP1\">\n" \
"      <Server>\n" \
"        <Authentication />\n" \
"        <LDevice inst=\"GenericIO\">\n" \
"          <LN0 lnClass=\"LLN0\" lnType=\"LLN01\" inst=\"\">\n" \
"\n";

static char XML_TEMPLATE_EventsDataSet_Header[] = \
"            <DataSet name=\"dsEvents\" desc=\"Events\">\n";

static char XML_TEMPLATE_EventsDataSet[] = \
"              <FCDA ldInst=\"GenericIO\" lnClass=\"GGIO\" fc=\"ST\" lnInst=\"1\" doName=\"%s\" daName=\"stVal\" />\n";

static char XML_TEMPLATE_MeasurementsDataSet_Header[] = \
"            <DataSet name=\"dsMeasurements\" desc=\"Measurements\">\n";

static char XML_TEMPLATE_MeasurementsDataSet[] = \
"              <FCDA ldInst=\"GenericIO\" lnClass=\"GGIO\" fc=\"MX\" lnInst=\"1\" doName=\"%s\" daName=\"mag.f\" />\n";

static char XML_TEMPLATE_DataSet_Footer[] = \
"            </DataSet>\n" \
"\n";

static char XML_TEMPLATE_EventsBufferedReportControlBlock[] = \
"            <ReportControl name=\"brcbEvents\" confRev=\"1\" datSet=\"dsEvents\" rptID=\"brEvents\" buffered=\"true\" intgPd=\"1000\" bufTime=\"50\">\n" \
"              <TrgOps period=\"true\" />\n" \
"              <OptFields seqNum=\"true\" timeStamp=\"true\" dataSet=\"true\" reasonCode=\"true\" configRef=\"true\" />\n" \
"              <RptEnabled max=\"1\" />\n" \
"            </ReportControl>\n" \
"\n";

static char XML_TEMPLATE_EventsUnbufferedReportControlBlock[] = \
"            <ReportControl name=\"urcbEvents\" confRev=\"1\" datSet=\"dsEvents\" rptID=\"rpEvents\" buffered=\"false\" intgPd=\"1000\" bufTime=\"50\">\n" \
"              <TrgOps period=\"true\" />\n" \
"              <OptFields seqNum=\"true\" timeStamp=\"true\" dataSet=\"true\" reasonCode=\"true\" configRef=\"true\" />\n" \
"              <RptEnabled max=\"1\" />\n" \
"            </ReportControl>\n" \
"\n";

static char XML_TEMPLATE_MeasurementsBufferedReportControlBlock[] = \
"            <ReportControl name=\"brcbMeasurements\" confRev=\"1\" datSet=\"dsMeasurements\" rptID=\"brMeasurements\" buffered=\"true\" intgPd=\"1000\" bufTime=\"50\">\n" \
"              <TrgOps period=\"false\" />\n" \
"              <OptFields seqNum=\"true\" timeStamp=\"true\" dataSet=\"true\" reasonCode=\"true\" entryID=\"true\" configRef=\"true\" />\n" \
"              <RptEnabled max=\"2\" />\n" \
"            </ReportControl>\n" \
"\n";

static char XML_TEMPLATE_MeasurementsUnbufferedReportControlBlock[] = \
"            <ReportControl name=\"urcbMeasurements\" confRev=\"1\" datSet=\"dsMeasurements\" rptID=\"rpMeasurements\" buffered=\"false\" intgPd=\"1000\" bufTime=\"50\">\n" \
"              <TrgOps period=\"false\" />\n" \
"              <OptFields seqNum=\"true\" timeStamp=\"true\" dataSet=\"true\" reasonCode=\"true\" entryID=\"true\" configRef=\"true\" />\n" \
"              <RptEnabled max=\"2\" />\n" \
"            </ReportControl>\n" \
"\n";

static char XML_TEMPLATE_EventsGSEControl[] = \
"            <GSEControl name=\"gcbEvents\" datSet=\"dsEvents\" appID=\"App\" confRev=\"1\"/>\n"\
"\n";

static char XML_TEMPLATE_MeasurementsGSEControl[] = \
"            <GSEControl name=\"gcbMeasurements\" datSet=\"dsMeasurements\" appID=\"App\" confRev=\"1\"/>\n"\
"\n";

static char XML_TEMPLATE_3[] = \
"            <DOI name=\"Mod\">\n" \
"              <DAI name=\"stVal\">\n" \
"              	<Val>on</Val>\n" \
"              </DAI>\n" \
"              <DAI name=\"ctlModel\">\n" \
"                <Val>status-only</Val>\n" \
"              </DAI>\n" \
"            </DOI>\n" \
"            <DOI name=\"Beh\">\n" \
"              <DAI name=\"stVal\">\n" \
"              	<Val>on</Val>\n" \
"              </DAI>\n" \
"            </DOI>\n" \
"            <DOI name=\"Health\">\n" \
"              <DAI name=\"stVal\">\n" \
"              	<Val>ok</Val>\n" \
"              </DAI>\n" \
"            </DOI>\n" \
"\n" \
"            <DOI name=\"NamPlt\">\n" \
"              <DAI name=\"vendor\">\n" \
"                <Val>" VENDOR "</Val>\n" \
"              </DAI>\n" \
"              <DAI name=\"swRev\">\n" \
"                <Val>" SWREV "</Val>\n" \
"              </DAI>\n" \
"              <DAI name=\"d\">\n" \
"                <Val>" DESCR "</Val>\n" \
"              </DAI>\n" \
"            </DOI>\n" \
"          </LN0>\n" \
"          <LN lnClass=\"LPHD\" lnType=\"LPHD1\" inst=\"1\" prefix=\"\">\n" \
"            <DOI name=\"PhyHealth\">\n" \
"              <DAI name=\"stVal\">\n" \
"              	<Val>ok</Val>\n" \
"              </DAI>\n" \
"            </DOI>\n" \
"          </LN>\n" \
"          <LN lnClass=\"GGIO\" lnType=\"GGIO1\" inst=\"1\" prefix=\"\">\n" \
"            <DOI name=\"Mod\">\n" \
"              <DAI name=\"stVal\">\n" \
"              	<Val>on</Val>\n" \
"              </DAI>\n" \
"              <DAI name=\"ctlModel\">\n" \
"                <Val>status-only</Val>\n" \
"              </DAI>\n" \
"            </DOI>\n" \
"            <DOI name=\"Beh\">\n" \
"              <DAI name=\"stVal\">\n" \
"              	<Val>on</Val>\n" \
"              </DAI>\n" \
"            </DOI>\n" \
"            <DOI name=\"Health\">\n" \
"              <DAI name=\"stVal\">\n" \
"              	<Val>ok</Val>\n" \
"              </DAI>\n" \
"            </DOI>\n";

static char XML_TEMPLATE_ctlModel [] = \
"            <DOI name=\"%s\"><DAI name=\"ctlModel\"><Val>direct-with-normal-security</Val></DAI></DOI>\n";

static char XML_TEMPLATE_4[] = \
"          </LN>\n" \
"        </LDevice>\n" \
"      </Server>\n" \
"    </AccessPoint>\n" \
"  </IED>\n" \
"  <DataTypeTemplates>\n" \
"\n" \
"    <LNodeType id=\"LLN01\" lnClass=\"LLN0\">\n" \
"      <DO name=\"Mod\" type=\"TMod\" />\n" \
"      <DO name=\"Beh\" type=\"TBeh\" />\n" \
"      <DO name=\"Health\" type=\"THealth\" />\n" \
"      <DO name=\"NamPlt\" type=\"TLLN0NamPlt\" />\n" \
"    </LNodeType>\n" \
"\n" \
"    <LNodeType id=\"LPHD1\" lnClass=\"LPHD\">\n" \
"      <DO name=\"PhyNam\" type=\"TPhyNam\" />\n" \
"      <DO name=\"PhyHealth\" type=\"THealth\" />\n" \
"      <DO name=\"Proxy\" type=\"TInd\" />\n" \
"    </LNodeType>\n" \
"\n" \
"    <LNodeType id=\"GGIO1\" lnClass=\"GGIO\">\n" \
"      <DO name=\"Mod\" type=\"TMod\" />\n" \
"      <DO name=\"Beh\" type=\"TBeh\" />\n" \
"      <DO name=\"Health\" type=\"THealth\" />\n" \
"      <DO name=\"NamPlt\" type=\"TNamPlt\" />\n";

static char XML_TEMPLATE_DO[] = \
"      <DO name=\"%s\" type=\"%s\" />\n";

static char XML_TEMPLATE_5[] = \
"    </LNodeType>\n" \
"\n" \
"    <DOType id=\"TMod\" cdc=\"ENC\">\n" \
"      <DA name=\"stVal\" bType=\"Enum\" type=\"EBeh\" fc=\"ST\" dchg=\"true\" />\n" \
"      <DA name=\"q\" bType=\"Quality\" fc=\"ST\" qchg=\"true\" />\n" \
"      <DA name=\"t\" bType=\"Timestamp\" fc=\"ST\" />\n" \
"      <DA name=\"ctlModel\" bType=\"Enum\" type=\"ECtlModels\" fc=\"CF\" />\n" \
"    </DOType>\n" \
"\n" \
"    <DOType id=\"TBeh\" cdc=\"ENS\">\n" \
"      <DA name=\"stVal\" bType=\"Enum\" type=\"EBeh\" fc=\"ST\" dchg=\"true\" />\n" \
"      <DA name=\"q\" bType=\"Quality\" fc=\"ST\" qchg=\"true\" />\n" \
"      <DA name=\"t\" bType=\"Timestamp\" fc=\"ST\" />\n" \
"    </DOType>\n" \
"\n" \
"    <DOType id=\"THealth\" cdc=\"ENS\">\n" \
"      <DA name=\"stVal\" bType=\"Enum\" type=\"EHealthKind\" fc=\"ST\" dchg=\"true\" />\n" \
"      <DA name=\"q\" bType=\"Quality\" fc=\"ST\" qchg=\"true\" />\n" \
"      <DA name=\"t\" bType=\"Timestamp\" fc=\"ST\" />\n" \
"    </DOType>\n" \
"\n" \
"    <DOType id=\"TLLN0NamPlt\" cdc=\"LPL\">\n" \
"      <DA name=\"vendor\" bType=\"VisString255\" fc=\"DC\" />\n" \
"      <DA name=\"swRev\" bType=\"VisString255\" fc=\"DC\" />\n" \
"      <DA name=\"d\" bType=\"VisString255\" fc=\"DC\" />\n" \
"      <DA name=\"configRev\" bType=\"VisString255\" fc=\"DC\" />\n" \
"      <DA name=\"ldNs\" bType=\"VisString255\" fc=\"EX\" />\n" \
"    </DOType>\n" \
"\n" \
"    <DOType id=\"TPhyNam\" cdc=\"DPL\">\n" \
"      <DA name=\"vendor\" bType=\"VisString255\" fc=\"DC\" />\n" \
"    </DOType>\n" \
"\n" \
"    <DOType id=\"TInd\" cdc=\"SPS\">\n" \
"      <DA name=\"stVal\" bType=\"BOOLEAN\" fc=\"ST\" dchg=\"true\" />\n" \
"      <DA name=\"q\" bType=\"Quality\" fc=\"ST\" qchg=\"true\" />\n" \
"      <DA name=\"t\" bType=\"Timestamp\" fc=\"ST\" />\n" \
"    </DOType>\n" \
"\n" \
"    <DOType id=\"TNamPlt\" cdc=\"LPL\">\n" \
"      <DA name=\"vendor\" bType=\"VisString255\" fc=\"DC\" />\n" \
"      <DA name=\"swRev\" bType=\"VisString255\" fc=\"DC\" />\n" \
"      <DA name=\"d\" bType=\"VisString255\" fc=\"DC\" />\n" \
"    </DOType>\n" \
"\n" \
"    <DOType id=\"TAnIn\" cdc=\"MV\">\n" \
"      <DA name=\"mag\" type=\"TMag\" bType=\"Struct\" fc=\"MX\" dchg=\"true\" />\n" \
"      <DA name=\"q\" bType=\"Quality\" fc=\"MX\" qchg=\"true\" />\n" \
"      <DA name=\"t\" bType=\"Timestamp\" fc=\"MX\" />\n" \
"    </DOType>\n" \
"\n" \
"    <DOType id=\"TSPC\" cdc=\"SPC\">\n" \
"      <DA name=\"stVal\" bType=\"BOOLEAN\" fc=\"ST\" dchg=\"true\" />\n" \
"      <DA name=\"q\" bType=\"Quality\" fc=\"ST\" qchg=\"true\" />\n" \
"      <DA name=\"ctlModel\" bType=\"Enum\" type=\"ECtlModels\" fc=\"CF\" />\n" \
"      <DA name=\"t\" bType=\"Timestamp\" fc=\"ST\" />\n" \
"	  <DA name=\"Oper\" type=\"TOper\" bType=\"Struct\" fc=\"CO\" />\n" \
"    </DOType>\n" \
"\n" \
"    <DAType id=\"TMag\">\n" \
"      <BDA name=\"f\" bType=\"FLOAT32\" />\n" \
"    </DAType>\n" \
"\n" \
"    <DAType id=\"TOrigin\">\n" \
"      <BDA name=\"orCat\" type=\"EOrCat\" bType=\"Enum\" />\n" \
"      <BDA name=\"orIdent\" bType=\"Octet64\" />\n" \
"    </DAType>\n" \
"\n" \
"    <DAType id=\"TOper\">\n" \
"      <BDA name=\"ctlVal\" bType=\"BOOLEAN\" />\n" \
"      <BDA name=\"origin\" type=\"TOrigin\" bType=\"Struct\" />\n" \
"      <BDA name=\"ctlNum\" bType=\"INT8U\" />\n" \
"      <BDA name=\"T\" bType=\"Timestamp\" />\n" \
"      <BDA name=\"Test\" bType=\"BOOLEAN\" />\n" \
"      <BDA name=\"Check\" bType=\"Check\" />\n" \
"    </DAType>\n" \
"\n" \
"    <EnumType id=\"EBeh\">\n" \
"      <EnumVal ord=\"1\">on</EnumVal>\n" \
"      <EnumVal ord=\"2\">blocked</EnumVal>\n" \
"      <EnumVal ord=\"3\">test</EnumVal>\n" \
"      <EnumVal ord=\"4\">test/blocked</EnumVal>\n" \
"      <EnumVal ord=\"5\">off</EnumVal>\n" \
"    </EnumType>\n" \
"\n" \
"    <EnumType id=\"EHealthKind\">\n" \
"	  <EnumVal ord=\"1\">ok</EnumVal>\n" \
"	  <EnumVal ord=\"2\">warning</EnumVal>\n" \
"	  <EnumVal ord=\"3\">alarm</EnumVal>\n" \
"    </EnumType>\n" \
"\n" \
"    <EnumType id=\"ECtlModels\">\n" \
"      <EnumVal ord=\"0\">status-only</EnumVal>\n" \
"      <EnumVal ord=\"1\">direct-with-normal-security</EnumVal>\n" \
"      <EnumVal ord=\"2\">sbo-with-normal-security</EnumVal>\n" \
"      <EnumVal ord=\"3\">direct-with-enhanced-security</EnumVal>\n" \
"      <EnumVal ord=\"4\">sbo-with-enhanced-security</EnumVal>\n" \
"    </EnumType>\n" \
"\n" \
"    <EnumType id=\"EOrCat\">\n" \
"      <EnumVal ord=\"0\">not-supported</EnumVal>\n" \
"      <EnumVal ord=\"1\">bay-control</EnumVal>\n" \
"      <EnumVal ord=\"2\">station-control</EnumVal>\n" \
"      <EnumVal ord=\"3\">remote-control</EnumVal>\n" \
"      <EnumVal ord=\"4\">automatic-bay</EnumVal>\n" \
"      <EnumVal ord=\"5\">automatic-station</EnumVal>\n" \
"      <EnumVal ord=\"6\">automatic-remote</EnumVal>\n" \
"      <EnumVal ord=\"7\">maintenance</EnumVal>\n" \
"      <EnumVal ord=\"8\">process</EnumVal>\n" \
"    </EnumType>\n" \
"\n" \
"  </DataTypeTemplates>\n" \
"</SCL>\n";

static void iec61850ExportScl(char* path, int include_com) {
	unsigned int i;
	FILE* fp;
	char name[16];
	uint8_t digitalPoints = 0;
	uint8_t analogPoints = 0;
	
	if( path == 0 || path[0] == 0 ) {
		return;
	}
	
	//Check to see if there are digital and/or analog points to export to IEC61850
	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].pnttype == PNT_DO || 
		    vars[i].pnttype == PNT_DI ) {
			digitalPoints = 1;
		}
		else if( vars[i].pnttype == PNT_AO || vars[i].pnttype == PNT_AO_SCALED ||
		    vars[i].pnttype == PNT_AI || vars[i].pnttype == PNT_AI_SCALED ) {
			analogPoints = 1;
		} 
	}
	
	fp = fopen(path,"w");
	if( fp == 0 ) {
		return;
	}
	
	fprintf(fp,"%s",XML_TEMPLATE_1);
	
	if( include_com ) {
		fprintf(fp,XML_TEMPLATE_COMMUNICATION_1,iec61850_serv_name);
		if( digitalPoints && iec61850_goose_digital_eth[0] ) {
			fprintf(fp,XML_TEMPLATE_COMMUNICATION_GSE,"gcbEvents",
					iec61850_goose_digital_dst[0]&0xFF,iec61850_goose_digital_dst[1]&0xFF,iec61850_goose_digital_dst[2]&0xFF,
					iec61850_goose_digital_dst[3]&0xFF,iec61850_goose_digital_dst[4]&0xFF,iec61850_goose_digital_dst[5]&0xFF,
					iec61850_goose_digital_appid);
		}
		if( analogPoints && iec61850_goose_analog_eth[0] ) {
			fprintf(fp,XML_TEMPLATE_COMMUNICATION_GSE,"gcbMeasurements",
					iec61850_goose_analog_dst[0]&0xFF,iec61850_goose_analog_dst[1]&0xFF,iec61850_goose_analog_dst[2]&0xFF,
					iec61850_goose_analog_dst[3]&0xFF,iec61850_goose_analog_dst[4]&0xFF,iec61850_goose_analog_dst[5]&0xFF,
					iec61850_goose_analog_appid);
		}
		fprintf(fp,"%s",XML_TEMPLATE_COMMUNICATION_2);
	}
	
	if( ! strlen(iec61850_serv_name) ) {
		fprintf(fp,XML_TEMPLATE_2,"TEMPLATE");
	}
	else {
		fprintf(fp,XML_TEMPLATE_2,iec61850_serv_name);
	}
	
	if( digitalPoints ) {
		fprintf(fp,"%s",XML_TEMPLATE_EventsDataSet_Header);
		for( i=0; i<VARSMAX; i++ ) {
			if( vars[i].pnttype == PNT_DO ) {
				snprintf(name,16,"SPCSO%d",vars[i].pntaddr);
				fprintf(fp,XML_TEMPLATE_EventsDataSet,name);
			}
			else if( vars[i].pnttype == PNT_DI ) {
				snprintf(name,16,"Ind%d",vars[i].pntaddr);
				fprintf(fp,XML_TEMPLATE_EventsDataSet,name);
			}
		}
		fprintf(fp,"%s",XML_TEMPLATE_DataSet_Footer);
	}
	
	if( analogPoints ) {
		fprintf(fp,"%s",XML_TEMPLATE_MeasurementsDataSet_Header);
		for( i=0; i<VARSMAX; i++ ) {
			if( vars[i].pnttype == PNT_AO || vars[i].pnttype == PNT_AO_SCALED) {
				snprintf(name,16,"AnOut%d",vars[i].pntaddr);
				fprintf(fp,XML_TEMPLATE_MeasurementsDataSet,name);
			}
			else if( vars[i].pnttype == PNT_AI || vars[i].pnttype == PNT_AI_SCALED ) {
				snprintf(name,16,"AnIn%d",vars[i].pntaddr);
				fprintf(fp,XML_TEMPLATE_MeasurementsDataSet,name);
			}
		}
		fprintf(fp,"%s",XML_TEMPLATE_DataSet_Footer);
	}
	
	if( digitalPoints ) {
		fprintf(fp,"%s",XML_TEMPLATE_EventsBufferedReportControlBlock);
		fprintf(fp,"%s",XML_TEMPLATE_EventsUnbufferedReportControlBlock);
	}
	
	if( analogPoints ) {
		fprintf(fp,"%s",XML_TEMPLATE_MeasurementsBufferedReportControlBlock);
		fprintf(fp,"%s",XML_TEMPLATE_MeasurementsUnbufferedReportControlBlock);
	}
	
	if( digitalPoints && iec61850_goose_digital_eth[0] ) {
		fprintf(fp,"%s",XML_TEMPLATE_EventsGSEControl);
	}
	
	if( analogPoints && iec61850_goose_analog_eth[0] ) {
		fprintf(fp,"%s",XML_TEMPLATE_MeasurementsGSEControl);
	}
	
	fprintf(fp,"%s",XML_TEMPLATE_3);
	
	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].pnttype == PNT_DO ) {
			snprintf(name,16,"SPCSO%d",vars[i].pntaddr);
			fprintf(fp,XML_TEMPLATE_ctlModel,name);
		}
	}
	fprintf(fp,"%s",XML_TEMPLATE_4);
	
	for( i=0; i<VARSMAX; i++ ) {
		if( vars[i].pnttype == PNT_DO ) {
			snprintf(name,16,"SPCSO%d",vars[i].pntaddr);
			fprintf(fp,XML_TEMPLATE_DO,name,"TSPC");
		}
		else if( vars[i].pnttype == PNT_DI ) {
			snprintf(name,16,"Ind%d",vars[i].pntaddr);
			fprintf(fp,XML_TEMPLATE_DO,name,"TInd");
		}
		else if( vars[i].pnttype == PNT_AO || vars[i].pnttype == PNT_AO_SCALED) {
			//snprintf(name,16,"AnOut%d",vars[i].pntaddr);
			//fprintf(fp,XML_TEMPLATE_DO,name,"TAnOut");
		}
		else if( vars[i].pnttype == PNT_AI || vars[i].pnttype == PNT_AI_SCALED ) {
			snprintf(name,16,"AnIn%d",vars[i].pntaddr);
			fprintf(fp,XML_TEMPLATE_DO,name,"TAnIn");
		}
	}
	fprintf(fp,"%s",XML_TEMPLATE_5);
	
	fclose(fp);
}

void iec61850ExportIcd() {
	iec61850ExportScl(iec61850_icd_path,0);
}

void iec61850ExportScd() {
	iec61850ExportScl(iec61850_scd_path,1);
}
