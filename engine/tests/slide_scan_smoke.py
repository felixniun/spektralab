"""Compare paper printing and direct film scan on a RAW copy.

This diagnoses stage selection; it does not claim parity with a Mac render.
Inputs are read only; all copies and output PNGs stay in --output.
"""
import argparse
import json
from pathlib import Path
import shutil
import struct
import zlib
from host_smoke import Host


def png(path, width, height, pixels):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    rows = b''.join(b'\0' + pixels[y * width * 4:(y + 1) * width * 4] for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


def main():
    ap = argparse.ArgumentParser()
    for arg in ('host', 'resources', 'raw', 'wire', 'output'):
        ap.add_argument('--' + arg, required=True)
    ap.add_argument('--workflow', choices=['slide', 'negative'], default='slide')
    args = ap.parse_args()
    out = Path(args.output)
    out.mkdir(parents=True, exist_ok=True)
    copy = out / Path(args.raw).name
    if Path(args.raw).resolve() == copy.resolve():
        raise ValueError('Use a separate output directory for the RAW copy')
    shutil.copyfile(args.raw, copy)
    params = json.loads(Path(args.wire).read_text(encoding='utf-8-sig'))
    stages = [('before-paper', False), ('after-direct-scan', True)] if args.workflow == 'slide' else [('before-direct-scan', True), ('after-paper', False)]
    params.update(scan_film=stages[0][1], preview_long_edge=1024)
    host = Host([args.host, '--resources', args.resources])
    def call(method, payload=None):
        header, pixels = host.call(method, payload)
        if not header.get('ok'):
            raise RuntimeError(str(header))
        return header['result'], pixels
    try:
        call('hello')
        opened, _ = call('open', {'path': str(copy.resolve()), 'params': params})
        sid = opened['session']
        result = {}
        previous = None
        for index, (label, direct) in enumerate(stages):
            if index:
                call('set_params', {'session': sid, 'delta': {'scan_film': direct}})
            render, pixels = call('render', {'session': sid, 'tier': 'live', 'format': 'rgba8', 'display': 'srgb', 'reprint': index > 0})
            image = render['image']
            w, h = image['width'], image['height']
            assert len(pixels) == w * h * 4
            png(out / (label + '.png'), w, h, pixels)
            # Report a region near the top, useful for inspecting this sky photo.
            samples = [pixels[(y * w + x) * 4:(y * w + x) * 4 + 3] for y in range(h // 20, h // 4) for x in range(w // 4, 3 * w // 4)]
            result[label] = {'size': [w, h], 'upper_region_mean_rgb': [round(sum(p[c] for p in samples) / len(samples), 2) for c in range(3)], 'reprinted': render.get('reprinted')}
            if index:
                assert pixels != previous, 'Switching scan_film did not change the rendered pixels'
            previous = pixels
        call('close', {'session': sid})
        (out / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
        print(json.dumps(result, indent=2), flush=True)
    finally:
        try:
            call('shutdown')
            host.p.wait(timeout=20)
        finally:
            if host.p.poll() is None:
                host.p.kill()


if __name__ == '__main__':
    main()
