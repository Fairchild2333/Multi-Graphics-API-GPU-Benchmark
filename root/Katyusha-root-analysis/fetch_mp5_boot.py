from pathlib import Path
import hashlib
import urllib.request

HERE = Path(__file__).resolve().parent
URL = ('https://dumps.tadiphone.dev/api/v4/projects/4705/repository/files/'
       'boot.img/raw?ref=0b3d76ba324b315f5e552221cd07def832c54bb3')
EXPECTED = '3d334299f223460222a66b9dd5f641d3f6c4099c5ed6660c88063468af7fe203'
SIZE = 201326592


def main():
    dest = HERE / 'MP5-images' / 'boot.img'
    dest.parent.mkdir(exist_ok=True)
    if dest.exists() and dest.stat().st_size == SIZE and hashlib.sha256(dest.read_bytes()).hexdigest() == EXPECTED:
        print('MP5 boot already verified', dest, flush=True)
        return
    partial = dest.with_suffix('.img.part')
    digest = hashlib.sha256()
    total = 0
    with urllib.request.urlopen(URL, timeout=30) as response, partial.open('wb') as output:
        assert response.status == 200 and int(response.headers['Content-Length']) == SIZE
        while True:
            data = response.read(1024 * 1024)
            if not data:
                break
            total += len(data)
            assert total <= SIZE
            digest.update(data)
            output.write(data)
            if total % (16 * 1024 * 1024) == 0:
                print(f'{total // (1024 * 1024)} / {SIZE // (1024 * 1024)} MiB', flush=True)
    assert total == SIZE and digest.hexdigest() == EXPECTED
    partial.replace(dest)
    print('MP5 source boot SHA256 VERIFIED', digest.hexdigest(), str(dest), flush=True)


if __name__ == '__main__':
    main()
