# SCADA-Simulator

The goal of this project is to create a generic SCADA/ICS simulation device.  Such devices can be used to expand testbeds by providing a large number of points that interact with each other is a predictable and realistic fashion.

The simulation is based upon a BASIC(ish) syntax where variables are defined based upon specified equations and resolved at regular time intervals.  Some of these variables are made visible via SCADA protocols.

Two prototype embedded devices (STM32 based) have been produced that support console and MODBUS via RS232.

A general PC code-base has been developed that support DOS and Linux targets.

DOS target support:
* MODBUS via RS232
* Console via RS232 (optional)
	
Linux targets support:
* MODBUS via RS232 (or serial over TCP/IP)
* MODBUS/TCP
* IEC61850/GOOSE
