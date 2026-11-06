from pathlib import Path
import base64
import collections
import hashlib
import io
import json
import struct
import urllib.request
import zipfile

OUT = Path(__file__).resolve().parent
URL = ('https://ota-1256119282.file.myqcloud.com/KTUS-A0/'
       '20221114150403-katyusha-deltaota_2022-10-31_2022-08-11_475bbf5ab3.zip')
SIZE = 1274288168


def get_range(start, length):
    request = urllib.request.Request(URL, headers={
        'Range': f'bytes={start}-{start + length - 1}',
        'User-Agent': 'FirmwareMetadataSearch',
    })
    with urllib.request.urlopen(request, timeout=30) as response:
        if response.status != 206:
            raise ValueError('Server did not honor the range request')
        expected = f'bytes {start}-{start + length - 1}/{SIZE}'
        if response.headers.get('Content-Range') != expected:
            raise ValueError('Unexpected Content-Range')
        data = response.read()
    if len(data) != length:
        raise ValueError('Incomplete range')
    return data


class RemoteZIP(io.RawIOBase):
    def __init__(self):
        self.position = 0

    def readable(self):
        return True

    def seekable(self):
        return True

    def tell(self):
        return self.position

    def seek(self, offset, whence=0):
        self.position = (offset if whence == 0 else
                         self.position + offset if whence == 1 else SIZE + offset)
        return self.position

    def read(self, length=-1):
        if length < 0:
            length = SIZE - self.position
        length = min(length, SIZE - self.position)
        if length <= 0:
            return b''
        data = get_range(self.position, length)
        self.position += length
        return data


def varint(data, offset):
    value = 0
    for shift in range(0, 70, 7):
        byte = data[offset]
        offset += 1
        value |= (byte & 127) << shift
        if not byte & 128:
            return value, offset
    raise ValueError('Invalid protobuf varint')


def protobuf(data):
    result = collections.defaultdict(list)
    offset = 0
    while offset < len(data):
        tag, offset = varint(data, offset)
        field, wire = tag >> 3, tag & 7
        if wire == 0:
            value, offset = varint(data, offset)
        elif wire == 2:
            size, offset = varint(data, offset)
            value = data[offset:offset + size]
            if len(value) != size:
                raise ValueError('Truncated protobuf field')
            offset += size
        elif wire in (1, 5):
            size = 8 if wire == 1 else 4
            value = data[offset:offset + size]
            offset += size
        else:
            raise ValueError(f'Unsupported protobuf wire type: {wire}')
        result[field].append(value)
    return result


def info(data):
    values = protobuf(data)
    return {'size': values.get(1, [0])[0],
            'sha256': values.get(2, [b''])[0].hex()}


def extent(data):
    values = protobuf(data)
    return {'start_block': values[1][0], 'num_blocks': values[2][0]}


def inspect():
    with zipfile.ZipFile(RemoteZIP()) as archive:
        metadata = archive.read('META-INF/com/android/metadata')
        properties = archive.read('payload_properties.txt')
        certificate = archive.read('META-INF/com/android/otacert')
        payload = archive.getinfo('payload.bin')
        if payload.compress_type != zipfile.ZIP_STORED:
            raise ValueError('Payload must be uncompressed in the ZIP')
        header = get_range(payload.header_offset, 30)
        name_size, extra_size = struct.unpack_from('<HH', header, 26)
        payload_start = payload.header_offset + 30 + name_size + extra_size
    (OUT / 'target-ota-metadata.txt').write_bytes(metadata)
    (OUT / 'payload_properties.txt').write_bytes(properties)
    (OUT / 'target-ota-certificate.pem').write_bytes(certificate)
    header = get_range(payload_start, 24)
    if header[:4] != b'CrAU' or struct.unpack_from('>Q', header, 4)[0] != 2:
        raise ValueError('Unsupported payload format')
    manifest_size = struct.unpack_from('>Q', header, 12)[0]
    signature_size = struct.unpack_from('>I', header, 20)[0]
    manifest = get_range(payload_start + 24, manifest_size)
    metadata_digest = base64.b64encode(hashlib.sha256(header + manifest).digest()).decode()
    props = dict(line.split('=', 1) for line in properties.decode().splitlines() if '=' in line)
    if metadata_digest != props['METADATA_HASH']:
        raise ValueError('Payload metadata SHA256 mismatch')
    (OUT / 'payload-manifest.bin').write_bytes(manifest)
    (OUT / 'payload-metadata.bin').write_bytes(header + manifest)
    values = protobuf(manifest)
    summary = {'url': URL, 'zip_size': SIZE, 'payload_start': payload_start,
               'payload_data_start': payload_start + 24 + manifest_size + signature_size,
               'manifest_size': manifest_size, 'metadata_sha256_verified': True,
               'block_size': values.get(3, [4096])[0],
               'minor_version': values.get(12, [0])[0], 'partitions': []}
    types = {0: 'REPLACE', 1: 'REPLACE_BZ', 4: 'SOURCE_COPY', 5: 'SOURCE_BSDIFF',
             6: 'ZERO', 7: 'DISCARD', 8: 'REPLACE_XZ', 9: 'PUFFDIFF',
             10: 'BROTLI_BSDIFF', 11: 'ZUCCHINI', 12: 'LZ4DIFF_BSDIFF',
             13: 'LZ4DIFF_PUFFDIFF'}
    for raw in values[13]:
        fields = protobuf(raw)
        partition = {'name': fields[1][0].decode(), 'operations': []}
        for key, number in [('old_info', 6), ('new_info', 7)]:
            if number in fields:
                partition[key] = info(fields[number][0])
        for op in fields.get(8, []):
            f = protobuf(op)
            partition['operations'].append({
                'type': f[1][0], 'type_name': types.get(f[1][0], str(f[1][0])),
                'data_offset': f.get(2, [0])[0], 'data_length': f.get(3, [0])[0],
                'data_sha256': f.get(8, [b''])[0].hex(),
                'src_extents': [extent(x) for x in f.get(4, [])],
                'dst_extents': [extent(x) for x in f.get(6, [])],
            })
        summary['partitions'].append(partition)
    (OUT / 'ota-manifest-summary.json').write_text(
        json.dumps(summary, indent=2), encoding='utf-8')
    print(metadata.decode())
    print('Metadata SHA256 verified:', metadata_digest)
    for p in summary['partitions']:
        counts = collections.Counter(x['type_name'] for x in p['operations'])
        print(p['name'], p.get('new_info'), dict(counts),
              'download_bytes=', sum(x['data_length'] for x in p['operations']))


if __name__ == '__main__':
    inspect()
