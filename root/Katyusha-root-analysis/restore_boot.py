from pathlib import Path
import hashlib
import json
import re

from verify_patch_candidates import apply_bsdf2

HERE = Path(__file__).resolve().parent


def main():
    summary = json.loads((HERE / 'ota-manifest-summary.json').read_text())
    part = next(p for p in summary['partitions'] if p['name'] == 'boot')
    source_path = HERE / 'MP5-images' / 'boot.img'
    source = source_path.read_bytes()
    assert len(source) == part['old_info']['size']
    assert hashlib.sha256(source).hexdigest() == part['old_info']['sha256']
    target = bytearray(part['new_info']['size'])
    block = summary['block_size']
    for op in part['operations']:
        data = b''.join(source[e['start_block'] * block:(e['start_block'] + e['num_blocks']) * block]
                        for e in op['src_extents'])
        if op['type_name'] == 'BROTLI_BSDIFF':
            patch = (HERE / 'MP5-to-MP6-boot.brotli-bsdiff').read_bytes()
            assert hashlib.sha256(patch).hexdigest() == op['data_sha256']
            data = apply_bsdf2(data, patch)
        elif op['type_name'] == 'ZERO':
            data = b'\0' * sum(e['num_blocks'] * block for e in op['dst_extents'])
        else:
            assert op['type_name'] == 'SOURCE_COPY'
        assert len(data) == sum(e['num_blocks'] * block for e in op['dst_extents'])
        offset = 0
        for e in op['dst_extents']:
            size = e['num_blocks'] * block
            target[e['start_block'] * block:(e['start_block'] + e['num_blocks']) * block] = data[offset:offset + size]
            offset += size
    digest = hashlib.sha256(target).hexdigest()
    assert digest == part['new_info']['sha256'], 'MP6 target SHA256 mismatch'
    dest = HERE / 'MP6-images' / 'boot.img'
    dest.parent.mkdir(exist_ok=True)
    dest.write_bytes(target)
    match = re.search(rb'Linux version [0-9][^\0]+', target)
    banner = match.group().decode('utf-8', errors='replace') if match else None
    result = {'path': str(dest), 'size': len(target), 'sha256': digest,
              'target_hash_verified': True, 'kernel_banner': banner,
              'source_url': 'https://dumps.tadiphone.dev/api/v4/projects/4705/repository/files/boot.img/raw?ref=0b3d76ba324b315f5e552221cd07def832c54bb3'}
    (HERE / 'MP6-boot-verification.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result), flush=True)


if __name__ == '__main__':
    main()
