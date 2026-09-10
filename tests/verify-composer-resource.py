"""Verify the actual compiled preview dialog, without loading plugin code."""
import struct
import sys
from pathlib import Path

data = Path(sys.argv[1]).read_bytes()


def u16(offset):
    return struct.unpack_from('<H', data, offset)[0]


def u32(offset):
    return struct.unpack_from('<I', data, offset)[0]


pe = u32(0x3C)
assert data[pe:pe + 4] == b'PE\0\0', 'Not a PE image'
optional = pe + 24
directories = optional + (112 if u16(optional) == 0x20B else 96)
sections = optional + u16(pe + 20)


def file_offset(rva):
    for i in range(u16(pe + 6)):
        section = sections + i * 40
        size, start, raw_size, raw = struct.unpack_from('<IIII', data, section + 8)
        if start <= rva < start + max(size, raw_size):
            return raw + rva - start
    raise AssertionError('Unmapped RVA')


root = file_offset(u32(directories + 16))


def entry(directory, resource_id=None):
    count = u16(directory + 12) + u16(directory + 14)
    for i in range(count):
        name, value = struct.unpack_from('<II', data, directory + 16 + 8 * i)
        if resource_id is None or name == resource_id:
            return root + (value & 0x7FFFFFFF)
    raise AssertionError('Missing resource')


leaf = entry(entry(entry(root, 5), 30500))  # RT_DIALOG / prompt composer / language
cursor = file_offset(u32(leaf))
assert u16(cursor) == 1 and u16(cursor + 2) == 0xFFFF, 'Expected DIALOGEX'
dialog_style = u32(cursor + 12)
count = u16(cursor + 16)
cursor += 26


def variable(offset):
    if u16(offset) == 0xFFFF:
        return u16(offset + 2), offset + 4
    start = offset
    while u16(offset):
        offset += 2
    return data[start:offset].decode('utf-16le'), offset + 2


for _ in range(3):
    _, cursor = variable(cursor)
if dialog_style & 0x40:  # DS_SETFONT
    cursor += 6
    _, cursor = variable(cursor)

controls = {}
for _ in range(count):
    cursor = (cursor + 3) & ~3
    style = u32(cursor + 8)
    x, y, width, height = struct.unpack_from('<hhhh', data, cursor + 12)
    identifier = u32(cursor + 20)
    cursor += 24
    klass, cursor = variable(cursor)
    _, cursor = variable(cursor)
    extra = u16(cursor)
    cursor += 2 + extra
    controls[identifier] = (klass, style, x, y, width, height)

policy = controls[30510]
label = controls[30506]
editor = controls[30507]
status = controls[30508]
assert policy[0] == 0x81, 'Policy must be an EDIT control to contain wrapped text'
for flag in (0x4, 0x800, 0x200000):  # multiline, readonly, vertical scroll
    assert policy[1] & flag, 'Policy must be multiline, readonly and scrollable'
assert policy[3] + policy[5] < label[3], 'Policy overlaps task label'
assert label[3] + label[5] < editor[3], 'Task label overlaps editor'
assert editor[3] + editor[5] < status[3], 'Editor overlaps status'
print('PASS: compiled policy region is readonly, multiline, scrollable and separated from task controls')
