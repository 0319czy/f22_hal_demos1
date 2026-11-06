#!/usr/bin/env python3
"""PIO -> vendor ProgISP 1.72 under Wine. Require real read/write/verify logs."""
import hashlib, json, os, pathlib, shutil, subprocess, sys, time

ROOT = pathlib.Path(__file__).resolve().parent
CACHE = pathlib.Path.home() / '.cache' / 'f22-progisp-pio'
PREFIX = pathlib.Path.home() / '.cache' / 'f22-progisp-wine'

def main():
    firmware = pathlib.Path(sys.argv[1]).resolve(strict=True)
    for tool in ['wine64', 'Xvfb', 'xdotool', 'x86_64-w64-mingw32-gcc']:
        if not shutil.which(tool):
            raise RuntimeError(f'Missing dependency: {tool}')
    vendor = os.environ.get('PROGISP_DIR')
    if vendor:
        vendor = pathlib.Path(vendor)
    else:
        repo = ROOT.parents[3]
        bundled = ROOT.parent / 'vendor'
        vendor = bundled if (bundled / 'progisp.exe').is_file() else repo / '竞赛相关技术资料/软件工具/烧录软件progisp1.72'
    if not (vendor / 'progisp.exe').is_file():
        raise RuntimeError('Set PROGISP_DIR to the vendor ProgISP 1.72 directory')
    CACHE.mkdir(parents=True, exist_ok=True)
    staged = CACHE / 'upload.hex'
    shutil.copyfile(firmware, staged)
    work = CACHE / 'vendor'
    shutil.copytree(vendor, work, dirs_exist_ok=True)
    ini = work / 'progisp.ini'
    text = ini.read_bytes().decode('gbk')
    replacements = {'CpuType': '1', 'PRGType': '0', 'Type': '2',
                    'ChipErase': '0', 'FlashProgram': '0', 'BlankCheck': '0',
                    'Fuse Program': '0', 'PreFuse Program': '0', 'LockChip': '0',
                    'EEPromProgram': '0', 'VerifyEEProm': '0', 'VerifyFlash': '1',
                    'SignatBox': '1', 'Savesize': '1', 'FormLeft': '10', 'FormTop': '10'}
    text = '\n'.join(k + '=' + replacements[k] if '=' in line and (k := line.split('=', 1)[0]) in replacements else line for line in text.splitlines())
    ini.write_bytes(text.encode('gbk'))
    probe = CACHE / 'probe.exe'
    subprocess.run(['x86_64-w64-mingw32-gcc', str(ROOT / 'progisp_probe.c'), '-o', str(probe)], check=True)
    env = dict(os.environ, WINEPREFIX=str(PREFIX), WINEDEBUG='-all')
    # This is a dedicated Wine prefix for this programmer only.
    subprocess.run(['/usr/lib/x86_64-linux-gnu/wine/wineserver', '-k'], env=env, check=False)
    rd, wr = os.pipe()
    xvfb = subprocess.Popen(['Xvfb', '-displayfd', str(wr), '-screen', '0', '1280x900x24', '-nolisten', 'tcp'], pass_fds=(wr,))
    os.close(wr)
    with os.fdopen(rd) as stream:
        display = stream.readline().strip()
    env['DISPLAY'] = ':' + display
    app = subprocess.Popen(['wine64', './progisp.exe'], cwd=work, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    def native(*args):
        return subprocess.run(['wine64', str(probe), *args], env=env, capture_output=True, text=True, timeout=5)
    def keys(*args):
        subprocess.run(['xdotool', *args], env=env, check=True)
    def click(x, y):
        keys('mousemove', str(x), str(y), 'click', '1')
    def wait_for(phrase, timeout=35):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            log = native('log').stdout
            if phrase.lower() in log.lower():
                print(log.strip(), flush=True)
                return log
            if any(s in log.lower() for s in ['failed', 'error', 'not found']):
                raise RuntimeError(log.strip())
            time.sleep(.2)
        raise RuntimeError(f'Timed out waiting for {phrase}: {native("log").stdout}')
    def file_dialog(path):
        time.sleep(.3)
        keys('key', 'alt+n', 'ctrl+a')
        keys('type', '--clearmodifiers', 'Z:' + str(path).replace('/', '\\'))
        keys('key', 'Return')
    try:
        deadline = time.monotonic() + 35
        while native('init').returncode:
            if time.monotonic() > deadline:
                raise RuntimeError('ProgISP window did not start')
            time.sleep(.2)
        wait_for('Programmer Ver:')
        native('clear')
        keys('key', 'alt+s')
        wait_for('Read ID Succesfully')
        native('clear')
        keys('key', 'alt+f')
        wait_for('Flash succesfully read')
        backup = CACHE / ('original-' + time.strftime('%Y%m%d-%H%M%S') + '.bin')
        click(782, 187)
        file_dialog(backup)
        deadline = time.monotonic() + 10
        while not backup.exists():
            if time.monotonic() > deadline:
                raise RuntimeError('Original firmware backup not saved; refusing erase')
            time.sleep(.2)
        if backup.stat().st_size != 8192:
            raise RuntimeError(f'Expected full 8192-byte backup, got {backup.stat().st_size}; refusing erase')
        print(f'Original firmware backup: {backup}', flush=True)
        native('clear')
        click(782, 115)
        file_dialog(staged)
        wait_for('Load Flash file')
        for name, on in [('Verify Signature', 1), ('Chip Erase', 1), ('Blank Check', 1), ('Program FLASH', 1), ('Verify FLASH', 1), ('Program Fuse', 0), ('Prewritten Fuse', 0), ('Lock Chip', 0), ('Program EEPROM', 0)]:
            native('check', name, str(on))
        native('clear')
        keys('key', 'alt+a')
        log = wait_for('Erase,Blank,Write Flash,Verify Flash,Successfully done', timeout=60)
        if 'succes' not in log.lower():
            raise RuntimeError('No successful FLASH verification evidence')
        receipt = {'firmware': str(firmware), 'sha256': hashlib.sha256(firmware.read_bytes()).hexdigest(), 'backup': str(backup), 'backup_sha256': hashlib.sha256(backup.read_bytes()).hexdigest(), 'progisp_result': log.strip(), 'time': time.strftime('%Y-%m-%d %H:%M:%S')}
        (CACHE / 'last-upload.json').write_text(json.dumps(receipt, ensure_ascii=False, indent=2))
        print('AT89S52 upload and FLASH verification passed.', flush=True)
    finally:
        subprocess.run(['/usr/lib/x86_64-linux-gnu/wine/wineserver', '-k'], env=env, check=False)
        app.wait(timeout=10)
        xvfb.terminate()
        xvfb.wait(timeout=5)

if __name__ == '__main__':
    try:
        main()
    except Exception as exc:
        print(f'Upload failed: {exc}', file=sys.stderr)
        sys.exit(1)
