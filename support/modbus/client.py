#!/usr/bin/env python3
## Copyright (c) 2022 Daniel Tabor
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
## 1. Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
## 2. Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution.
##
## THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
## ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
## ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
## FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
## DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
## OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
## HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
## LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
## OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
## SUCH DAMAGE.
##
import sys
import curses
import time
import traceback
from pymodbus.client.sync import ModbusSerialClient
from pymodbus.client.sync import ModbusTcpClient

dnames = [
	"ISO1",
	"ISO2",
	"BRK3",
	"BRK4",
	"ISO5",
	"ISO6",
	"BRK7",
	"ISO8"
]

anames = [
	"BB1V",
	"BB2V",
	"BRK3V",
	"BRK3I",
	"BRK7V",
	"BRK7I",
	"BB3V",
	"BB4V"
]

def usage():
	print("Usage:")
	print("%s [-h] [-a modbusAddress] [[-s serialDevice] | [-t ipSddress tcpPort]]" % sys.argv[0])
	print("")
	print("Default modbusAddress is 101")
	print("Either -s or -t must be specified")
	print("")
	sys.exit(1)

def main():
	client = None
	modbusAddress = 101
	i = 1
	while i < len(sys.argv):
		if sys.argv[i] == "-h":
			usage()
		elif sys.argv[i] == "-a":
			if i < len(sys.argv)-1:
				usage()
			try:
				modbusAddress = int(sys.argv[i+1])
			except ValueError:
				usage()
			i = i + 1
		elif sys.argv[i] == "-s":
			if client != None or i > len(sys.argv)-1:
				usage()
			client = ModbusSerialClient(method="rtu",port=sys.argv[i+1])
			i = i + 1
		elif sys.argv[i] == "-t":
			if client != None or i > len(sys.argv)-2:
				usage()
			try:
				port = int(sys.argv[i+2])
			except ValueError:
				usage()
			client = ModbusTcpClient(sys.argv[i+1],port)
			i = i + 2
		i = i + 1
	if client == None:
		usage()
	
	dv = [-1 for i in range(8)]
	av = [-1 for i in range(8)]
	
	try:
		stdscr = curses.initscr()
		curses.cbreak()
		curses.noecho()
		console = curses.newwin(17,36,0,0)
		messages = curses.newwin(stdscr.getmaxyx()[0],stdscr.getmaxyx()[1]-36,0,36);
		control = curses.newwin(4,36,18,0);
		console.nodelay(True);
		console.scrollok(True);
		messages.nodelay(True);
		messages.scrollok(True);
		control.nodelay(True);
		control.scrollok(True);

		next_console = time.time()
		next_control = next_console;
		paused = False

		while True:
			current_time = time.time()
			i = console.getch()
			if i != curses.ERR:
				if chr(i) == 'q' or chr(i) == 'Q':
					break
				elif chr(i) == ' ':
					if paused:
						control.clear()
						control.refresh()
						paused = False
					else:
						paused = True
						control.addstr(3,5,"PAUSED")
						control.refresh()
				elif not paused and current_time >= next_control and i >= ord('1') and i <= ord('8'):
					i = i - ord('0')
					control.clear();
					control.addstr(0,0,"Ctrl %s=%d" % (dnames[i-1],1-dv[i-1]))
					result = client.write_coil(i,bool(1-dv[i-1]),unit=modbusAddress)
					messages.addstr("%s\n" % str(result))
					messages.refresh()
					if result.isError():
						control.addstr(1,2,"Failed")
					else:
						control.addstr(1,2,"Success")
					control.refresh()
					next_control = current_time + 2.0

			if not paused and current_time >= next_control:
				control.clear()
				control.refresh()

			if not paused and current_time >= next_console:
				result = client.read_coils(1,8,unit=modbusAddress)
				messages.addstr("%s\n" % str(result))
				if not result.isError():
					for i in range(8):
						if result.getBit(i):
							dv[i] = 1
						else:
							dv[i] = 0
				result = client.read_input_registers(1,8,unit=modbusAddress)
				messages.addstr("%s\n" % str(result))
				if not result.isError():
					for i in range(8):
						av[i] = result.registers[i]
				messages.refresh()

				control.clear()
				for i in range(8):
					if dv[i] == -1:
						console.addstr(i,0,"[%d]%s: ?\n" % (i+1,dnames[i]))
					else:
						console.addstr(i,0,"[%d]%s: %d\n" % (i+1,dnames[i],dv[i]));
				for i in range(8):
					if av[i] == -1:
						console.addstr(8+i,0,"%s: ?\n" % (anames[i]))
					else:
						console.addstr(8+i,0,"%s: %0.3f\n" % (anames[i],av[i]))

				console.refresh()
				next_console = current_time + 2.0
			time.sleep(0.250)
		curses.endwin()
	except Exception:
		curses.endwin()
		traceback.print_exc()
	
	
if __name__ == "__main__":
	main()
	