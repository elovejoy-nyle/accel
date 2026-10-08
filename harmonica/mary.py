#!/usr/bin/env python3
import time
import pyvisa

IP = "192.168.1.34"
AMPLITUDE = 10.0  # Vpp, assuming a high-impedance load
BEAT = 0.35     # Seconds

notes = {
    "C": 261.63,
    "D": 293.66,
    "E": 329.63,
    "G": 392.00,
}

melody = [
    ("E", 1), ("D", 1), ("C", 1), ("D", 1),
    ("E", 1), ("E", 1), ("E", 2),
    ("D", 1), ("D", 1), ("D", 2),
    ("E", 1), ("G", 1), ("G", 2),
    ("E", 1), ("D", 1), ("C", 1), ("D", 1),
    ("E", 1), ("E", 1), ("E", 1), ("E", 1),
    ("D", 1), ("D", 1), ("E", 1), ("D", 1), ("C", 2),
]

rm = pyvisa.ResourceManager("@py")
fg = None

try:
    fg = rm.open_resource(f"TCPIP0::{IP}::5025::SOCKET")
    fg.read_termination = "\n"
    fg.write_termination = "\n"
    fg.timeout = 3000

    #fg.write("OUTP OFF")
    fg.write("OUTP:LOAD INF")
    fg.write("VOLT:UNIT VPP")
    fg.write(f"APPL:SIN {notes['E']},{AMPLITUDE},0")
    #fg.write("OUTP OFF")
    fg.query("*OPC?")

    for note, beats in melody:
        fg.write(f"FREQ {notes[note]}")
        fg.write("OUTP ON")
        fg.query("*OPC?")
        time.sleep(BEAT * beats * 0.9)
        #fg.write("OUTP OFF")
        time.sleep(BEAT * beats * 0.1)

finally:
    try:
        if fg is not None:
            try:
                fg.write("OUTP OFF")
            finally:
                fg.close()
    finally:
        rm.close()
