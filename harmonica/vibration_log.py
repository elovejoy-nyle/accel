#!/usr/bin/env python3
"""Log Arduino CSV lines alongside commanded Agilent 33220A stimulus settings.

Dependencies: pyserial, pyvisa, pyvisa-py.
Sensor columns are p2pX, p2pY, p2pZ, RMS(XYZ), converted to floats.
Times refer to complete-line receipt on the PC, not sensor acquisition time.
Generator settings are host annotations, not measured stimulus frequencies.
The generator is reset at startup. Output is disabled on exit.
"""
import argparse
import csv
from datetime import datetime, timezone
import math
import threading
import time

import pyvisa
import serial


# Edit these defaults here, or override them with command-line options.
IP = "192.168.1.34"
SERIAL_PORT = "/dev/ttyACM0"
BAUD_RATE = 115200
AMPLITUDE_VPP = 10.0         # High-impedance load setting
WAVEFORM = "SIN"            # SIN, SQU, RAMP, PULS
START_HZ = 1
STOP_HZ = 2000              # Included in the sweep when reached by the step
STEP_HZ = 1
TONE_SECONDS = 2.0
BREAK_SECONDS = 0.5
OUTPUT_OFF_BETWEEN_TONES = True
BASELINE_SECONDS = 2.0


def positive(value):
    number = float(value)
    if not math.isfinite(number) or number <= 0:
        raise argparse.ArgumentTypeError("must be a positive finite number")
    return number


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default=SERIAL_PORT)
    parser.add_argument("--baud", type=int, default=BAUD_RATE)
    parser.add_argument("--ip", default=IP)
    parser.add_argument("--start", type=int, default=START_HZ)
    parser.add_argument("--stop", type=int, default=STOP_HZ)
    parser.add_argument("--step", type=int, default=STEP_HZ)
    parser.add_argument("--frequencies", nargs="+", type=positive, default=None)
    parser.add_argument("--seconds", type=positive, default=TONE_SECONDS)
    parser.add_argument("--baseline", type=positive, default=BASELINE_SECONDS)
    parser.add_argument("--gap", type=float, default=BREAK_SECONDS)
    parser.add_argument("--toggle-output", action=argparse.BooleanOptionalAction,
                        default=OUTPUT_OFF_BETWEEN_TONES)
    parser.add_argument("--amplitude", type=positive, default=AMPLITUDE_VPP, help="Vpp at High Z")
    parser.add_argument("--waveform", choices=["SIN", "SQU", "RAMP", "PULS"], default=WAVEFORM)
    parser.add_argument("--output", default=None, help="new CSV filename; existing files refused")
    args = parser.parse_args()
    if args.start <= 0 or args.step <= 0 or args.stop < args.start:
        parser.error("require 0 < start <= stop and step > 0")
    if not math.isfinite(args.gap) or args.gap < 0:
        parser.error("gap must be finite and nonnegative")
    if args.frequencies is None:
        args.frequencies = list(range(args.start, args.stop + 1, args.step))
    date = datetime.now(timezone.utc).strftime("%Y-%m-%d_%H%M%S")
    filename = args.output or f"vibration_{date}.csv"

    stop = threading.Event()
    lock = threading.Lock()
    state = {"phase": "baseline", "output_on": 0, "commanded_frequency_hz": ""}
    errors = []
    rm = fg = ser = worker = None
    started = time.monotonic()

    def update(**values):
        with lock:
            state.update(values)

    def wait(seconds):
        if stop.wait(seconds):
            raise RuntimeError("Serial logger stopped") from errors[0]

    def change(command, **values):
        update(phase="transition")
        fg.write(command)
        fg.query("*OPC?")
        update(**values)

    def log_rows(log):
        writer = csv.writer(log)
        writer.writerow([
            "received_utc", "elapsed_s", "phase", "output_on", "waveform",
            "commanded_frequency_hz", "amplitude_vpp", "p2pX", "p2pY",
            "p2pZ", "RMS(XYZ)", "raw_line",
        ])
        pending = bytearray()
        try:
            while not stop.is_set():
                pending.extend(ser.read(min(max(ser.in_waiting, 1), 4096)))
                while b"\n" in pending:
                    line, _, remainder = pending.partition(b"\n")
                    pending = bytearray(remainder)
                    raw = line.rstrip(b"\r").decode("utf-8", errors="replace")
                    if not raw:
                        continue
                    received = datetime.now(timezone.utc).strftime("T%H:%M:%S.%f")[:-3]
                    elapsed = time.monotonic() - started
                    with lock:
                        settings = dict(state)
                    fields = raw.split(",")
                    try:
                        sensor = [float(value) for value in fields] if len(fields) == 4 else []
                        if len(sensor) != 4 or not all(math.isfinite(v) for v in sensor):
                            sensor = [""] * 4
                    except ValueError:
                        sensor = [""] * 4
                    writer.writerow([
                        received, f"{elapsed:.6f}", settings["phase"],
                        settings["output_on"], args.waveform,
                        settings["commanded_frequency_hz"], args.amplitude, *sensor, raw,
                    ])
        except Exception as exc:
            errors.append(exc)
            stop.set()

    try:
        with open(filename, "x", newline="", buffering=1) as log:
            rm = pyvisa.ResourceManager("@py")
            fg = rm.open_resource(f"TCPIP0::{args.ip}::5025::SOCKET")
            fg.read_termination = fg.write_termination = "\n"
            fg.timeout = 3000
            fg.write("*RST")
            fg.query("*OPC?")
            fg.write("OUTP OFF")
            fg.write("OUTP:LOAD INF")
            fg.write("VOLT:UNIT VPP")
            fg.write(f"FUNC {args.waveform}")
            fg.write(f"FREQ {args.frequencies[0]}")
            fg.write(f"VOLT {args.amplitude}")
            fg.write("VOLT:OFFS 0")
            fg.query("*OPC?")
            error = fg.query("SYST:ERR?").strip()
            if int(error.split(",", 1)[0]) != 0:
                raise RuntimeError(f"Generator setup: {error}")

            ser = serial.Serial(args.port, args.baud, timeout=0.05, exclusive=True)
            # Opening many Arduino boards resets them; allow startup before recording.
            time.sleep(2)
            ser.reset_input_buffer()
            started = time.monotonic()
            worker = threading.Thread(target=log_rows, args=(log,))
            worker.start()
            print(f"Logging to {filename}. Ctrl+C stops playback and recording.")
            try:
                wait(args.baseline)
                for index, frequency in enumerate(args.frequencies):
                    change(f"FREQ {frequency}", commanded_frequency_hz=frequency)
                    error = fg.query("SYST:ERR?").strip()
                    if int(error.split(",", 1)[0]) != 0:
                        raise RuntimeError(f"Generator frequency: {error}")
                    if not state["output_on"]:
                        change("OUTP ON", output_on=1, phase="tone")
                    else:
                        update(phase="tone")
                    print(f"{args.waveform}: {frequency:g} Hz for {args.seconds:g} s")
                    wait(args.seconds)
                    if index < len(args.frequencies) - 1:
                        if args.toggle_output:
                            change("OUTP OFF", output_on=0, commanded_frequency_hz="",
                                   phase="gap")
                        else:
                            update(phase="gap")
                        wait(args.gap)
                change("OUTP OFF", output_on=0, commanded_frequency_hz="", phase="baseline")
                wait(args.baseline)
            finally:
                stop.set()
                worker.join()
            if errors:
                raise RuntimeError("Serial logger failed") from errors[0]
    except KeyboardInterrupt:
        print("Stopped.")
    finally:
        stop.set()
        if worker is not None and worker.is_alive():
            worker.join()
        try:
            if fg is not None:
                try:
                    fg.write("OUTP OFF")
                finally:
                    fg.close()
        finally:
            if ser is not None:
                ser.close()
            if rm is not None:
                rm.close()
    print(f"Saved {filename}")


if __name__ == "__main__":
    main()
