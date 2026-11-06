"""Offline reconstruction experiment; writes images only on exact MP6 target SHA256.

BSDIFF format reference:
https://android.googlesource.com/platform/external/bsdiff/+/refs/heads/main/patch_reader.cc
https://android.googlesource.com/platform/external/bsdiff/+/refs/heads/main/utils.cc
"""
from pathlib import Path
import bz2
import hashlib
import json
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / 'tools' / 'pydeps'))
import brotli
import inspect_ota

SOURCE_DIR = Path(r'C:\Users\lzz28\Downloads\New folder\flash_signed_by_xiaomi_0080de01618842364d98743045ba419c\images')


def signed_int64(buf):
    value = int.from_bytes(buf, 'little')
    return -(value & ((1 << 63) - 1)) if value >> 63 else value


def apply_bsdf2(source, patch):
    assert len(patch) >= 32 and patch[:5] == b'BSDF2'
    control_size, diff_size, target_size = (signed_int64(patch[i:i + 8]) for i in (8, 16, 24))
    assert min(control_size, diff_size, target_size) >= 0
    assert 32 + control_size + diff_size <= len(patch)
    compressed = [patch[32:32 + control_size],
                  patch[32 + control_size:32 + control_size + diff_size],
                  patch[32 + control_size + diff_size:]]
    decompressors = {1: bz2.decompress, 2: brotli.decompress}
    control, diff, extra = [decompressors[k](b) for k, b in zip(patch[5:8], compressed)]
    assert len(control) % 24 == 0
    output = bytearray()
    old_position = diff_position = extra_position = 0
    for i in range(0, len(control), 24):
        x, y, z = (signed_int64(control[j:j + 8]) for j in (i, i + 8, i + 16))
        assert x >= 0 and y >= 0 and len(output) + x + y <= target_size
        delta = diff[diff_position:diff_position + x]
        assert len(delta) == x
        original = bytearray(x)
        left, right = max(0, old_position), min(len(source), old_position + x)
        if right > left:
            original[left - old_position:right - old_position] = source[left:right]
        output.extend((a + b) & 255 for a, b in zip(original, delta))
        output.extend(extra[extra_position:extra_position + y])
        assert extra_position + y <= len(extra)
        old_position += x + z
        diff_position += x
        extra_position += y
    assert len(output) == target_size
    assert diff_position == len(diff) and extra_position == len(extra)
    return bytes(output)


def main():
    summary = json.loads((HERE / 'ota-manifest-summary.json').read_text())
    manifest = inspect_ota.protobuf((HERE / 'payload-manifest.bin').read_bytes())
    raw_parts = {}
    for part in manifest[13]:
        values = inspect_ota.protobuf(part)
        raw_parts[values[1][0].decode()] = values
    results = []
    # Direct COS address has been verified to serve the identical object.
    inspect_ota.URL = summary['url'].replace('.file.myqcloud.com', '.cos.ap-guangzhou.myqcloud.com')
    for name, filename in [('boot', 'boot.img'), ('uefi', 'uefi.elf')]:
        part = next(p for p in summary['partitions'] if p['name'] == name)
        source = (SOURCE_DIR / filename).read_bytes()[:part['old_info']['size']]
        source += b'\0' * (part['old_info']['size'] - len(source))
        source_digest = hashlib.sha256(source).hexdigest()
        print(name, 'candidate source hash matches required base:', source_digest == part['old_info']['sha256'], flush=True)
        target = bytearray(part['new_info']['size'])
        block = summary['block_size']
        source_hash_matches = source_hash_total = 0
        for index, (op, raw_op) in enumerate(zip(part['operations'], raw_parts[name][8])):
            inputs = b''.join(source[e['start_block'] * block:(e['start_block'] + e['num_blocks']) * block]
                              for e in op['src_extents'])
            values = inspect_ota.protobuf(raw_op)
            if 9 in values:
                source_hash_total += 1
                source_hash_matches += hashlib.sha256(inputs).digest() == values[9][0]
            if op['type_name'] == 'SOURCE_COPY':
                data = inputs
            elif op['type_name'] == 'ZERO':
                data = b'\0' * sum(e['num_blocks'] * block for e in op['dst_extents'])
            elif op['type_name'] == 'BROTLI_BSDIFF':
                path = HERE / f'MP5-to-MP6-{name}.brotli-bsdiff'
                if path.exists():
                    patch = path.read_bytes()
                else:
                    print(name, 'fetching delta', op['data_length'], flush=True)
                    patch = inspect_ota.get_range(summary['payload_data_start'] + op['data_offset'], op['data_length'])
                assert hashlib.sha256(patch).hexdigest() == op['data_sha256']
                path.write_bytes(patch)
                data = apply_bsdf2(inputs, patch)
            else:
                raise ValueError(f'Unexpected operation {op["type_name"]}')
            assert len(data) == sum(e['num_blocks'] * block for e in op['dst_extents'])
            offset = 0
            for e in op['dst_extents']:
                size = e['num_blocks'] * block
                target[e['start_block'] * block:(e['start_block'] + e['num_blocks']) * block] = data[offset:offset + size]
                offset += size
        digest = hashlib.sha256(target).hexdigest()
        verified = digest == part['new_info']['sha256']
        result = {'partition': name, 'candidate_source_sha256': source_digest,
                  'source_operation_hashes_matched': source_hash_matches,
                  'source_operation_hashes_total': source_hash_total,
                  'result_sha256': digest, 'expected_sha256': part['new_info']['sha256'],
                  'target_hash_verified': verified}
        if verified:
            output = HERE / 'MP6-images' / (name + '.img')
            output.parent.mkdir(exist_ok=True)
            output.write_bytes(target)
            result['saved_path'] = str(output)
        results.append(result)
        print(json.dumps(result), flush=True)
    (HERE / 'candidate-patch-results.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
