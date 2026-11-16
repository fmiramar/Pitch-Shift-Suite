"""OSC/WAV helpers for offline SuperCollider example checks."""
import struct


def i32(n): return struct.pack('>i', n)


def pstr(s):
    b = s.encode(); return bytes([len(b)]) + b


def oscstr(s):
    b = s.encode() + b'\0'; return b + b'\0' * (-len(b) % 4)


def message(address, *args):
    tags, payload = ',', b''
    for arg in args:
        if isinstance(arg, bytes):
            tags += 'b'; payload += i32(len(arg)) + arg + b'\0' * (-len(arg) % 4)
        elif isinstance(arg, str):
            tags += 's'; payload += oscstr(arg)
        else:
            tags += 'i'; payload += i32(arg)
    return oscstr(address) + oscstr(tags) + payload


def bundle(time, msg):
    data = b'#bundle\0' + struct.pack('>Q', int(time * (1 << 32))) + i32(len(msg)) + msg
    return i32(len(data)) + data


def samples(path):
    b = path.read_bytes()
    if b[:4] != b'RIFF' or b[8:12] != b'WAVE': raise RuntimeError('Not RIFF WAVE')
    offset, fmt, data = 12, None, None
    while offset + 8 <= len(b):
        name, size = struct.unpack_from('<4sI', b, offset)
        part = b[offset+8:offset+8+size]
        if name == b'fmt ': fmt = struct.unpack_from('<HHIIHH', part)
        if name == b'data': data = part
        offset += 8 + size + size % 2
    if fmt is None or fmt[0] != 3 or fmt[1] != 2 or fmt[5] != 32 or data is None:
        raise RuntimeError('Expected stereo float32 WAV')
    return list(struct.iter_unpack('<ff', data))

