#!/usr/bin/env python3
"""
Dynamic presence-map codec with compression and original size tracking.
- Processes input in BLOCK_SIZE chunks.
- For each unique byte in the block:
  [1 byte: value][N bytes: presence map], where N = BLOCK_SIZE // 8
- Each block's encoded output is compressed with zlib.
- Original file size stored in first 8 bytes.
"""

import os, zlib
from rich.progress import Progress, BarColumn, TextColumn, TimeRemainingColumn, TimeElapsedColumn, TaskProgressColumn

BLOCK_SIZE = 1024  # ← Change this to 16, 64, 128, 256, etc.

MAP_BYTES = BLOCK_SIZE // 8  # 1 bit per position

def encode_block(block: bytes) -> bytes:
    out = bytearray()
    seen = set(block)
    for b in seen:
        out.append(b)
        bitmap = 0
        for i, val in enumerate(block):
            if val == b:
                bitmap |= (1 << (BLOCK_SIZE - 1 - i))
        out.extend(bitmap.to_bytes(MAP_BYTES, 'big'))
    return bytes(out)

def decode_block(data: bytes) -> bytes:
    out = bytearray([0] * BLOCK_SIZE)
    i = 0
    entry_size = 1 + MAP_BYTES
    while i + entry_size <= len(data):
        val = data[i]
        bitmap = int.from_bytes(data[i+1:i+1+MAP_BYTES], 'big')
        for j in range(BLOCK_SIZE):
            if (bitmap >> (BLOCK_SIZE - 1 - j)) & 1:
                out[j] = val
        i += entry_size
    return bytes(out)

def encode_file(in_path: str, out_path: str):
    original_size = os.path.getsize(in_path)
    with open(in_path, "rb") as f_in, open(out_path, "wb") as f_out, Progress(
        TextColumn("[bold cyan]Encoding[/]"),
        BarColumn(),
        TaskProgressColumn(),
        TimeElapsedColumn(),
        TimeRemainingColumn(),
    ) as progress:
        f_out.write(original_size.to_bytes(8, 'big'))
        task = progress.add_task("blocks", total=(original_size + BLOCK_SIZE - 1) // BLOCK_SIZE)
        while True:
            block = f_in.read(BLOCK_SIZE)
            if not block:
                break
            if len(block) < BLOCK_SIZE:
                block += bytes(BLOCK_SIZE - len(block))
            encoded = encode_block(block)
            compressed = zlib.compress(encoded)
            f_out.write(len(compressed).to_bytes(2, 'big'))
            f_out.write(compressed)
            progress.advance(task)
    print(f"Encoded {in_path} -> {out_path}")

def decode_file(in_path: str, out_path: str):
    with open(in_path, "rb") as f_in, open(out_path, "wb") as f_out, Progress(
        TextColumn("[bold green]Decoding[/]"),
        BarColumn(),
        TaskProgressColumn(),
        TimeElapsedColumn(),
        TimeRemainingColumn(),
    ) as progress:
        original_size = int.from_bytes(f_in.read(8), 'big')
        full_output = bytearray()
        task = progress.add_task("blocks", total=os.path.getsize(in_path))
        while True:
            len_bytes = f_in.read(2)
            if not len_bytes:
                break
            if len(len_bytes) < 2:
                raise ValueError("Truncated chunk length header.")
            chunk_len = int.from_bytes(len_bytes, 'big')
            compressed = f_in.read(chunk_len)
            if len(compressed) < chunk_len:
                raise ValueError(f"Incomplete chunk: expected {chunk_len}, got {len(compressed)}")
            encoded = zlib.decompress(compressed)
            block = decode_block(encoded)
            full_output.extend(block)
            progress.advance(task, advance=2 + chunk_len)
        f_out.write(full_output[:original_size])
    print(f"Decoded {in_path} -> {out_path}")

def main():
    import argparse
    ap = argparse.ArgumentParser(description="Presence-map codec with dynamic block size")
    ap.add_argument("mode", choices=["encode", "decode"], help="Choose 'encode' or 'decode'")
    ap.add_argument("input", help="Path to input file")
    ap.add_argument("output", help="Path to output file")
    args = ap.parse_args()

    if args.mode == "encode":
        encode_file(args.input, args.output)
    else:
        decode_file(args.input, args.output)

if __name__ == "__main__":
    main()
