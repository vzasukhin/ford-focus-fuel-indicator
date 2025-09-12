#!/usr/bin/env python

import argparse

import serial


HELLO_MESSAGE = b"\xaa\x03\x55\x39"
RESP_ACK = b"\x01"
RESP_NACK = b"\xa5"
CMD_FLASH = 0x1
CMD_RESET = 0x2
CMD_LOAD = 0x3

def send_hello(tty):
    tty.write(HELLO_MESSAGE)
    resp = tty.read(1)
    if resp != RESP_ACK:
        raise Exception(f"Wrong response for HELLO ({resp})")


def crc16(data):
    crc = 0xffff
    for x in data:
        crc ^= x << 8
        for _ in range(8):
            crc = ((crc << 1) & 0xffff) ^ 0x1021 if crc & 0x8000 else crc << 1

        crc &= 0xffff

    return crc


def flash_page(tty, data, offset):
    print(f"Flashing {offset:#x}, len: {len(data)}...")
    send_hello(tty)
    crc = crc16(data)
    hdr = bytes([CMD_FLASH, 0]) + int(len(data)).to_bytes(length=2, byteorder="little") + int(offset).to_bytes(length=2, byteorder="little") + int(crc).to_bytes(length=2, byteorder="little")
    a1=tty.write(hdr)

    resp = tty.read(1)
    if resp != RESP_ACK:
        raise Exception(f"Wrong response for header ({resp})")

    tty.write(data)

    resp = tty.read(1)
    if resp != RESP_ACK:
        raise Exception(f"Wrong response for data ({resp})")


def cmd_reset(tty):
    send_hello(tty)
    print("Send reset command")
    tty.write(bytes([CMD_RESET, 0, 0, 0, 0, 0, 0, 0]))
    resp = tty.read(1)
    if resp != RESP_ACK:
        raise Exception(f"Wrong response for reboot ({resp})")


def cmd_load(tty):
    send_hello(tty)
    print("Send load command")
    tty.write(bytes([CMD_LOAD, 0, 0, 0, 0, 0, 0, 0]))
    resp = tty.read(1)
    if resp != RESP_ACK:
        raise Exception(f"Wrong response for load ({resp})")


def wait_for_ready(tty):
    print("Waiting for bootloader start")
    tty.reset_input_buffer()
    data = b''
    while not data.endswith(b"Ready"):
        ch = tty.read(1)
        if not ch:
            continue

        data = data + ch

    send_hello(tty)



def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("fname", help="name of binary file with new firmware")
    parser.add_argument("-p", "--port", default="/dev/ttyUSB0", help="serial port")
    parser.add_argument("-b", "--baudrate", type=int, default=115200, help="speed of serial port")
    parser.add_argument("-l", "--load", action="store_true", help="do not reset after flashing... just continue loading")
    args = parser.parse_args()

    tty = serial.Serial(port=args.port, baudrate=args.baudrate, timeout=5)
    wait_for_ready(tty)

    offset = 0
    with open(args.fname, "rb") as f:
        while True:
            data = f.read(1024)
            if not data:
                break

            flash_page(tty, data, offset)
            offset += 1024

    if args.load:
        cmd_load(tty)
    else:
        cmd_reset(tty)


if __name__ == "__main__":
    main()
