#!/usr/bin/env python3
"""
EasyEXG Wi-Fi live viewer
=========================

Receives the UDP stream from an Arduino running EasyEXG_WiFi.ino and draws a
live, scrolling graph of the signal and its activity envelope.

The board sends packets of 25 samples, 20 times a second (500 Hz):
  signal   = ADC counts with the resting level removed, centred at 0 (mains notch applied)
  envelope = smoothed |signal|, i.e. muscle activity level (EMG)

Install:
  pip install matplotlib numpy

Run:
  python easyexg_wifi_plot.py                        # finds the board automatically (either Wi-Fi mode)
  python easyexg_wifi_plot.py --host 192.168.1.57    # board joined your router (IP from Serial Monitor)
  python easyexg_wifi_plot.py --host auto            # find the board by broadcast on your network
  python easyexg_wifi_plot.py --units uV --gain 1048 --vref 5.0 --csv ecg.csv
  python easyexg_wifi_plot.py --demo                 # fake signal, no board needed

Windows: the first time, allow Python through the firewall when asked (tick both
Private and Public, because the board's own network usually shows as Public).
"""

import argparse
import csv
import math
import socket
import struct
import sys
import threading
import time
from collections import deque

import numpy as np

MAGIC = b"EXG1"
HEADER = struct.Struct("<4sIHHB3x")      # magic, first index, count, fs, adc_bits
SAMPLE = struct.Struct("<ff")            # signal, envelope
DEFAULT_PORT = 5005

# Board address. "auto" finds the board by itself in both Wi-Fi modes
# (board's own "EasyEXG" network or your home router).
# If auto doesn't work, put the IP from the Serial Monitor here, e.g. "192.168.1.57".
# Handy when running from IDLE (F5), where you can't type --host.
BOARD_IP = "auto"


class Stream:
    """Thread-safe ring buffer shared by the receiver and the plot."""

    def __init__(self, seconds, fs=500):
        self.seconds = seconds
        self.lock = threading.Lock()
        self._alloc(fs)
        self.adc_bits = 10
        self.packets = 0
        self.lost_samples = 0
        self.next_index = None
        self.source = "-"
        self.rate_times = deque()
        self.last_packet_time = 0.0

    def _alloc(self, fs):
        self.fs = fs
        n = int(fs * self.seconds)
        self.sig = deque([0.0] * n, maxlen=n)
        self.env = deque([0.0] * n, maxlen=n)

    def push(self, first_index, fs, adc_bits, samples):
        now = time.time()
        with self.lock:
            if fs != self.fs:
                self._alloc(fs)
            self.adc_bits = adc_bits
            if self.next_index is not None:
                gap = (first_index - self.next_index) & 0xFFFFFFFF
                if 0 < gap < 0x7FFFFFFF:
                    self.lost_samples += gap
                    fill = min(gap, len(self.sig))   # keep the time axis honest
                    self.sig.extend([math.nan] * fill)
                    self.env.extend([math.nan] * fill)
            self.next_index = (first_index + len(samples)) & 0xFFFFFFFF
            for s, e in samples:
                self.sig.append(s)
                self.env.append(e)
            self.packets += 1
            self.last_packet_time = now
            self.rate_times.append((now, len(samples)))
            while self.rate_times and now - self.rate_times[0][0] > 2.0:
                self.rate_times.popleft()

    def snapshot(self):
        with self.lock:
            rate = sum(n for _, n in self.rate_times) / 2.0
            return (np.array(self.sig), np.array(self.env), self.fs, self.adc_bits,
                    rate, self.lost_samples, self.source, self.last_packet_time)


def parse_packet(data):
    if len(data) < HEADER.size:
        return None
    magic, first, count, fs, bits = HEADER.unpack_from(data)
    if magic != MAGIC or len(data) < HEADER.size + count * SAMPLE.size:
        return None
    samples = [SAMPLE.unpack_from(data, HEADER.size + i * SAMPLE.size) for i in range(count)]
    return first, fs, bits, samples


def receiver(stream, host, port, stop, csv_writer=None, verbose=True):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.bind(("", 0))
    sock.settimeout(0.3)
    # auto: say hello to the board's own-network address AND broadcast to the
    # whole local network, so it works whichever Wi-Fi mode the board is in
    targets = ["192.168.4.1", "255.255.255.255"] if host == "auto" else [host]
    if verbose:
        print(f"Saying HELLO to {' and '.join(targets)} (port {port}) ...")
    last_hello = 0.0
    warned = False
    t_start = time.time()
    while not stop.is_set():
        now = time.time()
        if now - last_hello > 1.0:
            for target in targets:
                try:
                    sock.sendto(b"HELLO", (target, port))
                except OSError:
                    pass      # e.g. 192.168.4.1 unreachable when on the home router
            last_hello = now
        if verbose and stream.packets == 0 and not warned and now - t_start > 5:
            print("\nNo data yet. Check:\n"
                  "  1. Serial Monitor (115200) shows 'Board IP: ...'. If it shows 'Wi-Fi network: EasyEXG',\n"
                  "     connect this PC to the EasyEXG Wi-Fi (password easyexg123).\n"
                  "     If it shows your router's name, the PC must be on that same router.\n"
                  "  2. Put that IP in BOARD_IP at the top of this script (or use --host IP).\n"
                  "  3. Windows firewall: allow Python on Private AND Public networks.\n"
                  "  4. The Serial Monitor can stay open: with Wi-Fi it doesn't block anything.")
            warned = True
        try:
            data, addr = sock.recvfrom(4096)
        except socket.timeout:
            continue
        except OSError:
            continue
        pkt = parse_packet(data)
        if pkt is None:
            continue
        first, fs, bits, samples = pkt
        if stream.packets == 0:
            if verbose:
                print(f"Receiving from {addr[0]} ({fs} Hz, {bits}-bit ADC)")
            if host == "auto":           # lock on to the board once found
                targets = [addr[0]]
        stream.source = addr[0]
        stream.push(first, fs, bits, samples)
        if csv_writer is not None:
            for i, (s, e) in enumerate(samples):
                csv_writer.writerow([f"{(first + i) / fs:.4f}", f"{s:.3f}", f"{e:.3f}"])
    sock.close()


def demo_source(stream, stop):
    """Fake EMG-ish signal: bursts of noise with a 50 Hz hum, 500 Hz."""
    fs, idx = 500, 0
    rng = np.random.default_rng(0)
    stream.source = "demo"
    while not stop.is_set():
        t = (idx + np.arange(25)) / fs
        burst = 1.0 + 6.0 * (np.sin(2 * np.pi * 0.4 * t) > 0.6)
        sig = rng.normal(0, 4, 25) * burst
        env_prev = stream.env[-1] if len(stream.env) and not math.isnan(stream.env[-1]) else 0.0
        env = []
        for s in sig:
            env_prev += (abs(s) - env_prev) * 0.02
            env.append(env_prev)
        stream.push(idx, fs, 10, list(zip(sig.tolist(), env)))
        idx += 25
        time.sleep(0.05)


def main():
    ap = argparse.ArgumentParser(description="EasyEXG Wi-Fi live viewer")
    ap.add_argument("--host", default=BOARD_IP,
                    help="board IP, or 'auto' (default) to find it in either Wi-Fi mode")
    ap.add_argument("--port", type=int, default=DEFAULT_PORT, help="board UDP port (default 5005)")
    ap.add_argument("--seconds", type=float, default=5.0, help="width of the scrolling window")
    ap.add_argument("--units", choices=["counts", "uV"], default="counts")
    ap.add_argument("--gain", type=float, default=1.0, help="amplifier gain, for --units uV")
    ap.add_argument("--vref", type=float, default=5.0, help="ADC reference voltage (UNO R4: 5.0, ESP32: 3.3)")
    ap.add_argument("--csv", help="also save samples to this CSV file")
    ap.add_argument("--demo", action="store_true", help="fake signal, no board needed")
    args = ap.parse_args()

    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation

    stream = Stream(args.seconds)
    stop = threading.Event()

    csv_file = writer = None
    if args.csv:
        csv_file = open(args.csv, "w", newline="")
        writer = csv.writer(csv_file)
        writer.writerow(["time_s", "signal_counts", "envelope_counts"])

    if args.demo:
        th = threading.Thread(target=demo_source, args=(stream, stop), daemon=True)
    else:
        th = threading.Thread(target=receiver, args=(stream, args.host, args.port, stop, writer), daemon=True)
    th.start()

    unit = "µV" if args.units == "uV" else "ADC counts"
    fig, (ax1, ax2) = plt.subplots(2, 1, sharex=True, figsize=(11, 6))
    (l_sig,) = ax1.plot([], [], lw=0.8, color="#2563eb")
    (l_env,) = ax2.plot([], [], lw=1.4, color="#dc2626")
    ax1.set_ylabel(f"signal ({unit})")
    ax2.set_ylabel(f"envelope ({unit})")
    ax2.set_xlabel("time (s)")
    for ax in (ax1, ax2):
        ax.grid(alpha=0.3)
    ax1.set_xlim(-args.seconds, 0)
    title = fig.suptitle("Waiting for data ...")

    def scale(bits):
        if args.units == "counts":
            return 1.0
        return args.vref / (2 ** bits) / args.gain * 1e6

    def update(_):
        sig, env, fs, bits, rate, lost, src, last = stream.snapshot()
        k = scale(bits)
        sig, env = sig * k, env * k
        x = (np.arange(len(sig)) - len(sig)) / fs
        l_sig.set_data(x, sig)
        l_env.set_data(x, env)
        for ax, y in ((ax1, sig), (ax2, env)):
            y = y[~np.isnan(y)]
            if y.size:
                lo, hi = float(y.min()), float(y.max())
                pad = max((hi - lo) * 0.1, 1e-6 if args.units == "uV" else 1.0)
                ax.set_ylim(lo - pad, hi + pad)
        if stream.packets == 0:
            title.set_text("Waiting for data ...  (see the console for tips)")
        else:
            stale = time.time() - last > 1.0
            title.set_text(f"{src}  |  {rate:.0f} samples/s  |  lost {lost} samples"
                           + ("  |  NO DATA - connection lost?" if stale else ""))
        return l_sig, l_env, title

    anim = FuncAnimation(fig, update, interval=40, blit=False, cache_frame_data=False)
    try:
        plt.show()
    finally:
        stop.set()
        th.join(timeout=1)
        if csv_file:
            csv_file.close()
            print(f"Saved {args.csv}")
    return anim


if __name__ == "__main__":
    main()
