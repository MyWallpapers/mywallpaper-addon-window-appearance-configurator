"""Test the real companion against a test-owned, separate Win32 process."""
import ctypes
import json
import os
import pathlib
import queue
import shutil
import struct
import subprocess
import sys
import tempfile
import threading
import time

root = pathlib.Path(__file__).resolve().parents[1]
build = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else root / '.build' / 'Release'
user32 = ctypes.WinDLL('user32', use_last_error=True)
user32.GetWindowLongPtrW.argtypes = [ctypes.c_void_p, ctypes.c_int]
user32.GetWindowLongPtrW.restype = ctypes.c_ssize_t
user32.GetLayeredWindowAttributes.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_ubyte), ctypes.POINTER(ctypes.c_uint32)]
user32.GetWindow.argtypes = [ctypes.c_void_p,ctypes.c_uint]
user32.GetWindow.restype = ctypes.c_void_p
user32.GetWindowRect.argtypes = [ctypes.c_void_p,ctypes.c_void_p]
user32.PostMessageW.argtypes = [ctypes.c_void_p,ctypes.c_uint,ctypes.c_size_t,ctypes.c_ssize_t]
user32.IsZoomed.argtypes = [ctypes.c_void_p]
user32.IsIconic.argtypes = [ctypes.c_void_p]
user32.GetDpiForWindow.argtypes = [ctypes.c_void_p]
user32.GetClassNameW.argtypes=[ctypes.c_void_p,ctypes.c_wchar_p,ctypes.c_int]
user32.IsWindowVisible.argtypes=[ctypes.c_void_p]
callback_type=ctypes.WINFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_ssize_t)
user32.EnumWindows.argtypes=[callback_type,ctypes.c_ssize_t]
checks = []

def check(test, message):
    if not test:
        raise AssertionError(message)
    checks.append(message)

def until(predicate, message):
    deadline = time.monotonic() + 6
    while time.monotonic() < deadline:
        if predicate():
            checks.append(message)
            return
        time.sleep(.04)
    raise AssertionError(message)

def send(process, value, chunk=1024*1024):
    data = json.dumps(value, ensure_ascii=False).encode('utf8')
    pieces = [data[i:i+chunk] for i in range(0, len(data), chunk)]
    for index, part in enumerate(pieces):
        kind = 0 if len(pieces) == 1 else 1 if index == 0 else 3 if index == len(pieces)-1 else 2
        process.stdin.write(struct.pack('<I', (kind << 30) | len(part)) + part)
    process.stdin.flush()

def launch():
    process = subprocess.Popen([str(build / 'companion.exe')], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=subprocess.CREATE_NO_WINDOW)
    events = queue.Queue()
    def read():
        try:
            while True:
                header = process.stdout.read(4)
                if not header:
                    break
                raw, = struct.unpack('<I', header)
                length = raw & 0x3fffffff
                body = process.stdout.read(length)
                events.put(json.loads(body))
        except BaseException as error:
            events.put(error)
    threading.Thread(target=read, daemon=True).start()
    return process, events

def expect(events, type_, code=None):
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        item = events.get(timeout=max(.1, deadline-time.monotonic()))
        if isinstance(item, BaseException):
            raise item
        if item['type'] == type_ and (code is None or item.get('code') == code):
            return item
        if item['type'] == 'error':
            raise AssertionError(item)
    raise AssertionError(f'No {type_}')

fixture = subprocess.Popen([str(build / 'window_appearance_tests.exe'), '--fixture'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=subprocess.CREATE_NO_WINDOW)
companions = []
secondary = None
scratch_context=tempfile.TemporaryDirectory(prefix='mw-appearance-wire-',ignore_cleanup_errors=True)
try:
    identity = json.loads(fixture.stdout.readline())
    hwnd = identity['hwnd']
    original = identity['style']
    original_ex = identity['ex']
    def style():
        return user32.GetWindowLongPtrW(hwnd, -16)
    def alpha():
        key = ctypes.c_uint32()
        opacity = ctypes.c_ubyte()
        flags = ctypes.c_uint32()
        if not user32.GetLayeredWindowAttributes(hwnd, ctypes.byref(key), ctypes.byref(opacity), ctypes.byref(flags)):
            return None
        return opacity.value
    with scratch_context as scratch:
        secondary_path = pathlib.Path(scratch) / 'appearance_secondary_fixture.exe'
        shutil.copyfile(build / 'window_appearance_tests.exe', secondary_path)
        secondary = subprocess.Popen([str(secondary_path), '--fixture'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=subprocess.CREATE_NO_WINDOW)
        second = json.loads(secondary.stdout.readline())
        def second_style():
            return user32.GetWindowLongPtrW(second['hwnd'], -16)
        def second_alpha():
            key, opacity, flags = ctypes.c_uint32(), ctypes.c_ubyte(), ctypes.c_uint32()
            return opacity.value if user32.GetLayeredWindowAttributes(second['hwnd'],ctypes.byref(key),ctypes.byref(opacity),ctypes.byref(flags)) else None
        values = {'targetApps':'window_appearance_tests.exe', 'profile':'aurora', 'windowOpacity':85, 'inactiveOpacity':85, 'rule1Enabled':True,'rule1Apps':'appearance_secondary_fixture.exe','rule1Profile':'graphite','rule1Opacity':70}
        init = {'type':'init','v':5,'addonId':'dev-a06-wire','artifactDigest':'sha256:'+'a'*64,'version':json.loads((root / 'manifest.json').read_text(encoding='utf8'))['version'],'targetArchitecture':'windows-x86_64','scratchDirectory':scratch,'layerSettings':{},'deviceSettings':values}
        process, events = launch()
        companions.append(process)
        # Real split framing, without shrinking the public maximum chunk length.
        send(process, init, chunk=73)
        expect(events, 'ready')
        until(lambda: style() & 0xC00000 == 0 and alpha() == 216, 'split init applies title + opacity to a separate Win32 host')
        until(lambda: second_style() & 0xC00000 == 0 and second_alpha() == 178, 'second executable receives its own profile / opacity rule')
        check(identity['pid'] != second['pid'], 'two distinct application processes managed together')
        def title_button(button):
            title_handles=[]
            @callback_type
            def find_title(window, _):
                cls=ctypes.create_unicode_buffer(256)
                user32.GetClassNameW(window,cls,256)
                if cls.value=='MyWallpaper.Appearance.Decoration' and user32.GetWindow(window,4)==hwnd:
                    title_handles.append(window)
                return True
            def title_ready():
                title_handles.clear()
                user32.EnumWindows(find_title,0)
                return len(title_handles)==1 and user32.IsWindowVisible(title_handles[0])!=0
            until(title_ready,'test-owned companion title becomes visible')
            check(len(title_handles)==1,'exact test-owned companion title found')
            rect=(ctypes.c_long*4)()
            user32.GetWindowRect(title_handles[0],ctypes.byref(rect))
            scale=user32.GetDpiForWindow(hwnd)/96
            size=int(36*scale)
            x=int(rect[2]-rect[0]-size*(2.5-button))
            y=int((rect[3]-rect[1])/2)
            point=(y<<16)|(x&65535)
            user32.PostMessageW(title_handles[0],0x201,1,point)
            user32.PostMessageW(title_handles[0],0x202,0,point)
        title_button(1)
        until(lambda: user32.IsZoomed(hwnd)!=0,'companion Maximize button controls its host')
        until(lambda: style() & 0xC00000 == 0xC00000,'maximized host retains its original caption')
        user32.PostMessageW(hwnd,0x112,0xF120,0)
        until(lambda: user32.IsZoomed(hwnd)==0 and style() & 0xC00000 == 0,'restoring host reapplies companion title')
        title_button(0)
        until(lambda: user32.IsIconic(hwnd)!=0,'companion Minimize button controls its host')
        user32.PostMessageW(hwnd,0x112,0xF120,0)
        until(lambda: user32.IsIconic(hwnd)==0 and style() & 0xC00000 == 0,'restoring minimized host reapplies its profile')
        duplicate, duplicate_events = launch()
        companions.append(duplicate)
        expect(duplicate_events, 'error', 'addon_instance_conflict')
        check(duplicate.wait(timeout=4) != 0, 'second session rejected')
        send(process, {'type':'settings','v':5,'layerSettings':{},'deviceSettings':{'windowOpacity':1}})
        expect(events, 'error', 'addon_settings_rejected')
        check(alpha() == 216 and style() & 0xC00000 == 0, 'invalid settings retain accepted appearance')
        send(process, {'type':'settings','v':5,'layerSettings':{},'deviceSettings':{'enabled':False}})
        until(lambda: style() == original and user32.GetWindowLongPtrW(hwnd,-20) == original_ex, 'disable restores styles and alpha mode')
        until(lambda: second_style() == second['style'] and user32.GetWindowLongPtrW(second['hwnd'],-20) == second['ex'], 'disable restores the second host independently')
        send(process, {'type':'settings','v':5,'layerSettings':{},'deviceSettings':values})
        until(lambda: style() & 0xC00000 == 0, 'reactivate after disable')
        send(process, {'type':'shutdown','v':5})
        check(process.wait(timeout=6) == 0, 'explicit shutdown succeeds')
        until(lambda: style() == original, 'explicit shutdown restores')
        until(lambda: not list(pathlib.Path(scratch).glob('window-appearance-*.json')), 'restoration journal removed')
        for mode in ('eof','kill'):
            process, events = launch()
            companions.append(process)
            send(process, init)
            expect(events, 'ready')
            until(lambda: style() & 0xC00000 == 0 and alpha() == 216, f'{mode} session applied')
            if mode == 'eof':
                process.stdin.close()
            else:
                process.kill()
            process.wait(timeout=6)
            until(lambda: style() == original and user32.GetWindowLongPtrW(hwnd,-20) == original_ex, f'{mode} restores through companion / independent guardian')
            until(lambda: second_style() == second['style'] and user32.GetWindowLongPtrW(second['hwnd'],-20) == second['ex'], f'{mode} also restores the second host')
            until(lambda: not list(pathlib.Path(scratch).glob('window-appearance-*.json')), f'{mode} journal cleared')
        process,events=launch();companions.append(process);send(process,init);expect(events,'ready')
        until(lambda: style() & 0xC00000 == 0,'close-button session applied')
        title_button(2)
        check(fixture.wait(timeout=6)==0,'companion Close button closes only its test-owned host')
        check(second_style() & 0xC00000 == 0,'second host remains managed after first host closes')
        send(process,{'type':'shutdown','v':5});check(process.wait(timeout=6)==0,'shutdown after target destruction succeeds')
        until(lambda: second_style()==second['style'],'remaining host restored after target destruction')
        malformed, events = launch()
        companions.append(malformed)
        malformed.stdin.write(struct.pack('<I', (2 << 30) | 2) + b'{}')
        malformed.stdin.flush()
        expect(events, 'error', 'companion_error')
        check(malformed.wait(timeout=5) != 0, 'orphan continuation rejected')
        secondary.stdin.write(b'quit\n'); secondary.stdin.flush(); secondary.wait(timeout=6); secondary = None
    result = {'checks':checks,'count':len(checks),'scope':'separate test-owned Win32 host, real companion process-v2, actual forced owner termination and guardian recovery','windows':sys.getwindowsversion().build}
    (root / 'evidence' / 'wire-result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    print(f'{len(checks)} integration checks passed')
finally:
    for process in companions:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)
    if secondary is not None:
        secondary.stdin.write(b'quit\n'); secondary.stdin.flush(); secondary.wait(timeout=6)
    if fixture.poll() is None:
        fixture.stdin.write(b'quit\n')
        fixture.stdin.flush()
        fixture.wait(timeout=6)
    check(pathlib.Path(scratch_context.name).resolve().parent==pathlib.Path(tempfile.gettempdir()).resolve(),'temporary cleanup stays in explicit test directory')
    scratch_context.cleanup()
