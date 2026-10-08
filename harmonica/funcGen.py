#!/usr/bin/env python3
import time
import pyvisa

rm = pyvisa.ResourceManager("@py")
fg = None

try:
    fg = rm.open_resource("TCPIP0::192.168.1.34::5025::SOCKET")
    fg.read_termination = "\n"
    fg.write_termination = "\n"
    fg.timeout = 3000

    fg.write("OUTP OFF")
    fg.write("OUTP:LOAD INF")
    fg.write("VOLT:UNIT VPP")
    fg.write("APPL:SIN 2000,1,0")
    fg.query("*OPC?")
    time.sleep(2)
finally:
    try:
        if fg is not None:
            try:
                fg.write("OUTP OFF")
            finally:
                fg.close()
    finally:
        rm.close()
