from pathlib import Path
import hashlib
import json
import lzma

import inspect_ota

HERE = Path(__file__).resolve().parent
SOURCE = Path(r'C:\Users\lzz28\Downloads\New folder\flash_signed_by_xiaomi_0080de01618842364d98743045ba419c\images\xbl_config.elf')


def main():
    summary = json.loads((HERE / 'ota-manifest-summary.json').read_text())
    part = next(p for p in summary['partitions'] if p['name'] == 'xbl_config')
    source = SOURCE.read_bytes()[:part['old_info']['size']]
    assert len(source) == part['old_info']['size']
    assert hashlib.sha256(source).hexdigest() == part['old_info']['sha256']
    target = bytearray(part['new_info']['size'])
    block = summary['block_size']
    for op in part['operations']:
        if op['type_name'] == 'SOURCE_COPY':
            data = b''.join(source[e['start_block'] * block:(e['start_block'] + e['num_blocks']) * block]
                            for e in op['src_extents'])
        elif op['type_name'] == 'REPLACE_XZ':
            blob = inspect_ota.get_range(summary['payload_data_start'] + op['data_offset'], op['data_length'])
            assert hashlib.sha256(blob).hexdigest() == op['data_sha256']
            (HERE / 'MP6-xbl_config-replace.xz').write_bytes(blob)
            data = lzma.decompress(blob)
        else:
            raise ValueError(f'Unsupported operation {op["type_name"]}')
        assert len(data) == sum(e['num_blocks'] * block for e in op['dst_extents'])
        offset = 0
        for e in op['dst_extents']:
            length = e['num_blocks'] * block
            target[e['start_block'] * block:(e['start_block'] + e['num_blocks']) * block] = data[offset:offset + length]
            offset += length
    digest = hashlib.sha256(target).hexdigest()
    assert digest == part['new_info']['sha256'], 'Target image hash mismatch'
    dest = HERE / 'MP6-images' / 'xbl_config.img'
    dest.parent.mkdir(exist_ok=True)
    dest.write_bytes(target)
    print(json.dumps({'path': str(dest), 'size': len(target), 'sha256': digest, 'target_hash_verified': True}))


if __name__ == '__main__':
    main()
