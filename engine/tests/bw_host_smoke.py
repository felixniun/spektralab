"""Verify the product B&W profiles through the packaged Windows host."""
import argparse
import json
from pathlib import Path
import shutil
import time
from host_smoke import Host
from slide_scan_smoke import png


def main():
    parser = argparse.ArgumentParser()
    for name in ('host', 'resources', 'raw', 'wire', 'output'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    raw = output / Path(args.raw).name
    if raw.resolve() == Path(args.raw).resolve():
        raise ValueError('Use a separate output directory')
    shutil.copyfile(args.raw, raw)
    cases = json.loads(Path(args.wire).read_text(encoding='utf-8-sig'))
    host = Host([args.host, '--resources', args.resources])
    def call(method, params=None):
        header, payload = host.call(method, params)
        if not header.get('ok'):
            raise RuntimeError(str(header))
        return header['result'], payload
    records = []
    try:
        call('hello')
        opened, _ = call('open', {'path': str(raw.resolve()), 'params': cases[0]})
        sid = opened['session']
        for index, params in enumerate(cases):
            if index:
                call('set_params', {'session': sid, 'delta': params})
            start = time.perf_counter()
            result, pixels = call('render', {'session': sid, 'tier': 'live', 'format': 'rgba8', 'display': 'srgb'})
            elapsed = round((time.perf_counter() - start) * 1000, 1)
            w, h = result['image']['width'], result['image']['height']
            spread = max(max(pixels[i:i + 3]) - min(pixels[i:i + 3]) for i in range(0, len(pixels), 4))
            assert spread <= 2, f"{params['film_stock']} is not neutral B&W: channel spread {spread}"
            assert max(pixels[0::4]) - min(pixels[0::4]) > 30, 'Flat/empty print'
            png(output / (params['film_stock'] + '.png'), w, h, pixels)
            records.append({'film': params['film_stock'], 'paper': params['print_stock'], 'size': [w, h], 'max_channel_spread': spread, 'render_roundtrip_ms': elapsed})
        call('close', {'session': sid})
        (output / 'result.json').write_text(json.dumps(records, indent=2), encoding='utf-8')
        print(json.dumps(records, indent=2), flush=True)
    finally:
        try:
            call('shutdown')
            host.p.wait(timeout=20)
        finally:
            if host.p.poll() is None:
                host.p.kill()


if __name__ == '__main__':
    main()
