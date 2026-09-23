#!/usr/bin/env python3
"""Long-running UART0 RS485 echo probe with simultaneous UART1 diagnostics."""

import argparse
import datetime as dt
import json
import pathlib
import struct
import sys
import time


def crc16_modbus(data: bytes) -> int:
    value = 0xFFFF
    for byte in data:
        value ^= byte
        for _ in range(8):
            value = (value >> 1) ^ (0xA001 if value & 1 else 0)
    return value


def make_frame(sequence: int, length: int) -> bytes:
    if not 16 <= length <= 256:
        raise ValueError("frame length must be 16..256")
    body = bytearray(b"U0T!")
    body += struct.pack("<I", sequence & 0xFFFFFFFF)
    body += struct.pack("<I", (~sequence) & 0xFFFFFFFF)
    for index in range(length - len(body) - 2):
        body.append((sequence * 37 + index * 73 + 0x5A) & 0xFF)
    body += struct.pack("<H", crc16_modbus(body))
    return bytes(body)


def utc_now() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat(timespec="milliseconds")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port0", required=True, help="PC RS485 adapter connected to UART0, e.g. COM5")
    parser.add_argument("--port1", required=True, help="PC USB-TTL adapter connected to UART1, e.g. COM6")
    parser.add_argument("--baud0", type=int, default=115200)
    parser.add_argument("--baud1", type=int, default=115200)
    parser.add_argument("--interval", type=float, default=1.0, help="seconds between probes")
    parser.add_argument("--timeout", type=float, default=0.7, help="echo timeout in seconds")
    parser.add_argument("--length", type=int, default=16, help="test frame length, 16..256")
    parser.add_argument("--duration", type=float, default=0.0, help="seconds; 0 runs until Ctrl-C")
    parser.add_argument("--ready-timeout", type=float, default=10.0,
                        help="seconds to wait for UART1 to report UART0 RX ready")
    parser.add_argument("--settle", type=float, default=0.5,
                        help="seconds to wait after the first ready status")
    parser.add_argument("--split-after", type=int, default=0,
                        help="optional byte offset for deliberate inter-byte gap")
    parser.add_argument("--split-gap-ms", type=float, default=0.0)
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()

    if (not 16 <= args.length <= 256 or args.interval <= 0 or args.timeout <= 0 or
            args.ready_timeout <= 0 or args.settle < 0):
        parser.error("length must be 16..256; interval, timeout and ready-timeout must be positive")
    if args.split_after and not 0 < args.split_after < args.length:
        parser.error("split-after must be within the frame")
    if args.split_gap_ms < 0:
        parser.error("split-gap-ms cannot be negative")
    try:
        import serial
    except ImportError:
        print("Missing pyserial. Install with: python -m pip install pyserial", file=sys.stderr)
        return 2

    directory = args.output or pathlib.Path("uart0_probe_" + dt.datetime.now().strftime("%Y%m%d_%H%M%S"))
    directory.mkdir(parents=True, exist_ok=False)
    (directory / "settings.json").write_text(json.dumps(vars(args), default=str, indent=2), encoding="utf-8")
    print(f"Logs: {directory.resolve()}", flush=True)
    print("Do not run another serial monitor on these two COM ports at the same time.", flush=True)

    with serial.Serial(args.port0, args.baud0, timeout=0, write_timeout=2) as port0, \
         serial.Serial(args.port1, args.baud1, timeout=0, write_timeout=2) as port1, \
         (directory / "events.jsonl").open("w", encoding="utf-8", buffering=1) as events, \
         (directory / "uart1_raw.bin").open("wb", buffering=0) as raw1:

        def record(kind: str, **fields: object) -> None:
            events.write(json.dumps({"utc": utc_now(), "monotonic": time.monotonic(),
                                     "kind": kind, **fields}, ensure_ascii=False) + "\n")

        record("start", port0=args.port0, port1=args.port1, baud0=args.baud0, baud1=args.baud1)
        start = time.monotonic()
        next_send = float("inf")
        ready_at = None
        failed_ready = False
        sequence = 0
        pending = None
        diag_buffer = bytearray()
        totals = {"ok": 0, "mismatch": 0, "timeout": 0, "extra": 0}

        try:
            while not args.duration or time.monotonic() - start < args.duration:
                now = time.monotonic()

                chunk1 = port1.read(port1.in_waiting or 1)
                if chunk1:
                    raw1.write(chunk1)
                    diag_buffer.extend(chunk1)
                    while b"\n" in diag_buffer:
                        line, _, remainder = diag_buffer.partition(b"\n")
                        diag_buffer = bytearray(remainder)
                        message = line.rstrip(b"\r").decode("ascii", errors="replace")
                        record("uart1", text=message)
                        if (ready_at is None and message.startswith("U0SD I1 R1 T0 ") and
                                " CMD2/2/" in message and " rem256/" in message):
                            ready_at = time.monotonic() + args.settle
                            next_send = ready_at
                            record("ready", settle_seconds=args.settle, status=message)
                            print(f"[{utc_now()}] UART0 RX ready; first probe in {args.settle:g} s", flush=True)
                        if message.startswith("U0EV ") and any(
                            marker in message for marker in (" K3 ", " K4 ", " K6 ")
                        ):
                            print(f"[{utc_now()}] {message}", flush=True)
                    if len(diag_buffer) > 4096:
                        record("uart1_unterminated", hex=diag_buffer.hex().upper())
                        diag_buffer.clear()

                chunk0 = port0.read(port0.in_waiting or 1)
                if chunk0:
                    record("uart0_rx_chunk", hex=chunk0.hex().upper())
                    if pending is None:
                        totals["extra"] += 1
                        record("unsolicited", hex=chunk0.hex().upper())
                        print(f"[{utc_now()}] unsolicited UART0: {chunk0.hex(' ').upper()}", flush=True)
                    else:
                        pending["received"].extend(chunk0)
                        if len(pending["received"]) >= len(pending["expected"]):
                            got = bytes(pending["received"])
                            expected = pending["expected"]
                            result = "ok" if got == expected else "mismatch"
                            totals[result] += 1
                            record("result", seq=pending["seq"], result=result,
                                   expected=expected.hex().upper(), got=got.hex().upper(),
                                   latency_ms=round((time.monotonic() - pending["sent_at"]) * 1000, 3))
                            if result != "ok":
                                print(f"[{utc_now()}] seq={pending['seq']} MISMATCH "
                                      f"expected={expected.hex().upper()} got={got.hex().upper()}", flush=True)
                            elif totals["ok"] % 60 == 0:
                                print(f"[{utc_now()}] OK={totals['ok']} "
                                      f"timeout={totals['timeout']} mismatch={totals['mismatch']}", flush=True)
                            pending = None

                now = time.monotonic()
                if ready_at is None and now - start >= args.ready_timeout:
                    failed_ready = True
                    record("ready_timeout", seconds=args.ready_timeout)
                    print(f"[{utc_now()}] UART0 ready status not seen on {args.port1}; "
                          "check UART1 wiring, baud rate and firmware", flush=True)
                    break
                if pending is not None and now >= pending["deadline"]:
                    totals["timeout"] += 1
                    record("result", seq=pending["seq"], result="timeout",
                           expected=pending["expected"].hex().upper(),
                           got=bytes(pending["received"]).hex().upper(),
                           latency_ms=round((now - pending["sent_at"]) * 1000, 3))
                    print(f"[{utc_now()}] seq={pending['seq']} TIMEOUT "
                          f"partial={bytes(pending['received']).hex().upper()}", flush=True)
                    pending = None

                if pending is None and now >= next_send:
                    frame = make_frame(sequence, args.length)
                    # The optional split mode intentionally tests the MCU's idle-line framing threshold.
                    if args.split_after:
                        port0.write(frame[:args.split_after])
                        port0.flush()
                        time.sleep(args.split_gap_ms / 1000.0)
                        port0.write(frame[args.split_after:])
                    else:
                        port0.write(frame)
                    port0.flush()
                    sent_at = time.monotonic()
                    record("send", seq=sequence, hex=frame.hex().upper())
                    pending = {"seq": sequence, "expected": frame, "received": bytearray(),
                               "sent_at": sent_at, "deadline": sent_at + args.timeout}
                    sequence += 1
                    next_send = max(next_send + args.interval, sent_at + args.interval)

                time.sleep(0.002)
        except KeyboardInterrupt:
            pass
        finally:
            if pending is not None:
                record("interrupted", seq=pending["seq"],
                       partial=bytes(pending["received"]).hex().upper())
            record("stop", totals=totals)
            print(f"Stopped. Totals: {totals}; logs: {directory.resolve()}", flush=True)
    return 1 if failed_ready else 0


if __name__ == "__main__":
    raise SystemExit(main())
