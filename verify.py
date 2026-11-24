"""Verify each UF2 block against its Intel HEX build and the T1 application partition."""
import binascii
import hashlib
import json
import zipfile
from pathlib import Path
import struct
import sys

def verify(uf2, hexfile):
    firmware = Path(uf2).read_bytes()
    if not firmware or len(firmware) % 512:
        raise ValueError("Incomplete UF2")
    payload = {}
    count = len(firmware) // 512
    for n in range(count):
        block = firmware[n*512:(n+1)*512]
        magic1, magic2, flags, addr, size, number, total, family = struct.unpack_from('<8I', block)
        if (magic1, magic2, flags, number, total, family) != (0x0a324655, 0x9e5d5157, 0x2000, n, count, 0xada52840):
            raise ValueError(f"Unexpected header at UF2 block {n}")
        if block[-4:] != bytes.fromhex('306fb10a') or size != 256 or addr % 256 or addr < 0x26000 or addr+size > 0xd4000:
            raise ValueError(f"Block {n} exceeds the T1 application layout or is malformed")
        if addr in payload:
            raise ValueError("Duplicate UF2 block")
        payload[addr] = block[32:32+size]

    expected = {}
    upper = 0
    eof = False
    for line in Path(hexfile).read_text().splitlines():
        raw = bytes.fromhex(line[1:])
        if eof or not line.startswith(':') or len(raw) != raw[0]+5 or sum(raw)%256:
            raise ValueError("Invalid HEX")
        addr = int.from_bytes(raw[1:3], 'big')
        kind = raw[3]
        data = raw[4:-1]
        if kind == 4:
            upper = int.from_bytes(data,'big') << 16
        elif kind == 2:
            upper = int.from_bytes(data,'big') << 4
        elif kind == 0:
            for i, b in enumerate(data, upper+addr):
                if i < 0x26000 or i >= 0xd4000: raise ValueError("HEX outside app")
                expected.setdefault(i & ~255, bytearray([255]) * 256)[i % 256] = b
        elif kind == 1:
            eof = True
        elif kind not in (3,5):
            raise ValueError(f"Unexpected HEX record type {kind}")
    if not eof or payload != expected: raise ValueError("UF2 and built HEX differ")
    sp, reset = struct.unpack_from('<II',payload[0x26000])
    if not 0x20006000 < sp <= 0x20040000 or reset % 2 != 1 or (reset & ~255) not in payload:
        raise ValueError("Invalid T1 application vectors")
    return dict(bytes=len(firmware), blocks=count, start=hex(min(payload)),
                end=hex(max(payload)+255), sha256=hashlib.sha256(firmware).hexdigest())

def verify_dfu(package, uf2):
    """Check the Nordic app payload, hardware declaration, and transfer CRC."""
    image = Path(uf2).read_bytes()
    pages = {struct.unpack_from('<I', image, i+12)[0]: image[i+32:i+288]
             for i in range(0,len(image),512)}
    if min(pages)!=0x26000 or max(pages)+256>0xd4000:
        raise ValueError("Unexpected UF2 application range")
    contents = b''.join(pages.get(addr, b'\xff'*256)
                        for addr in range(min(pages),max(pages)+1,256))
    with zipfile.ZipFile(package) as z:
        manifest = json.loads(z.read("manifest.json"))["manifest"]
        if set(manifest)!={"application","dfu_version"} or z.testzip() is not None:
            raise ValueError("Expected a valid application-only DFU package")
        app=manifest["application"]
        binary=z.read(app["bin_file"])
        init=app["init_packet_data"]
        if not binary or len(binary)>len(contents) or contents != binary.ljust(len(contents),b'\xff'):
            raise ValueError("Nordic and UF2 firmware differ")
        if init["device_type"]!=82 or init["softdevice_req"]!=[182] or init["firmware_crc16"]!=binascii.crc_hqx(binary,0xffff):
            raise ValueError("Nordic target or CRC mismatch")
        device, revision, version, count, softdevice, crc = struct.unpack("<HHIHHH",z.read(app["dat_file"]))
        if (device, revision, version, count, softdevice, crc)!=(82,init["device_revision"],init["application_version"],1,182,init["firmware_crc16"]):
            raise ValueError("Nordic initialization data disagree with manifest")
    return hashlib.sha256(Path(package).read_bytes()).hexdigest()

def verify_usb_entry(elf_path):
    """Read defined ELF function symbols to distinguish linked code from debug strings."""
    elf = Path(elf_path).read_bytes()
    if elf[:6] != b"\x7fELF\x01\x01":
        raise ValueError("Expected little-endian ELF32 for the T1")
    offset = struct.unpack_from("<I", elf, 32)[0]
    size, count = struct.unpack_from("<HH", elf, 46)
    sections = [struct.unpack_from("<10I", elf, offset + i * size) for i in range(count)]
    functions = set()
    for section in sections:
        if section[1] != 2:  # SHT_SYMTAB
            continue
        names = sections[section[6]]
        strings = elf[names[4]:names[4]+names[5]]
        for idx in range(section[4], section[4]+section[5], section[9]):
            name, _, _, info, _, defined = struct.unpack_from("<IIIBBH", elf, idx)
            if defined and info & 15 == 2:
                functions.add(strings[name:strings.index(0, name)])
    if not {b"__wrap_enterSerialDfu", b"enterUf2Dfu"} <= functions or b"enterSerialDfu" in functions:
        raise ValueError("USB reset does not resolve to the UF2 entry point")

if __name__ == '__main__':
    print(verify(*sys.argv[1:]))
