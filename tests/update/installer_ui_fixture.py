"""Opt-in real NSIS wizard checks, restricted to a QTemporaryDir installation."""
import ctypes
from ctypes import wintypes
import subprocess
import time

from flow_fixture import process_exited, wait_for


def repair_with_running_app(artifact, root, work, pid, confirm):
    user = ctypes.WinDLL('user32', use_last_error=True)
    callback = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user.EnumWindows.argtypes = [callback, wintypes.LPARAM]
    user.EnumChildWindows.argtypes = [wintypes.HWND, callback, wintypes.LPARAM]
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.GetWindow.argtypes = [wintypes.HWND, wintypes.UINT]
    user.GetWindow.restype = wintypes.HWND
    user.GetDlgItem.argtypes = [wintypes.HWND, ctypes.c_int]
    user.GetDlgItem.restype = wintypes.HWND
    user.GetDlgCtrlID.argtypes = [wintypes.HWND]
    user.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    user.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    user.IsWindowVisible.argtypes = [wintypes.HWND]
    user.IsWindowEnabled.argtypes = [wintypes.HWND]
    user.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    user.SendMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    user.SendMessageW.restype = ctypes.c_ssize_t

    def label(hwnd, class_name=False):
        value = ctypes.create_unicode_buffer(4096)
        (user.GetClassNameW if class_name else user.GetWindowTextW)(hwnd, value, len(value))
        return value.value

    def windows():
        found = []

        @callback
        def visit(hwnd, _):
            if user.IsWindowVisible(hwnd) and label(hwnd, True) == '#32770':
                owner_pid = wintypes.DWORD()
                user.GetWindowThreadProcessId(hwnd, ctypes.byref(owner_pid))
                found.append((hwnd, owner_pid.value))
            return True

        user.EnumWindows(visit, 0)
        return found

    def children(hwnd):
        found = []

        @callback
        def visit(child, _):
            if user.IsWindowVisible(child):
                found.append((child, label(child), label(child, True)))
            return True

        user.EnumChildWindows(hwnd, visit, 0)
        return found

    # Always suppress registration/shortcuts during interactive verification.
    # /D is the final unquoted NSIS tail, including spaces and Unicode.
    child = subprocess.Popen('"' + str(artifact.resolve()) + '" /NOINTEGRATION /D=' + str(root), cwd=work)
    wizard = None
    answered = False
    finish = False
    last_page = None
    observed = []
    try:
        wizard = wait_for(lambda: next((h for h, p in windows() if p == child.pid), None), 10)
        deadline = time.monotonic() + 60
        while child.poll() is None and time.monotonic() < deadline:
            dialogs = [h for h, _ in windows()
                       if user.GetWindow(h, 4) == wizard and label(h) == 'Daymark 安装']
            if dialogs and not answered:
                assert not process_exited(pid), 'application exited before confirmation'
                # Custom TaskDialog button ID 101; IDCANCEL=2. The helper owns
                # the dialog and NSIS wizard is its validated owner HWND.
                assert user.PostMessageW(dialogs[0], 0x111, 101 if confirm else 2, 0)
                answered = True
            elif user.IsWindowEnabled(wizard) and (not answered or (confirm and process_exited(pid))):
                next_button = user.GetDlgItem(wizard, 1)
                controls = children(wizard)
                run = [h for h, text, cls in controls if cls == 'Button' and text == '运行 Daymark']
                if run:
                    assert confirm and answered, 'unexpected completion before confirmation'
                    # Never let the wizard start Daymark on a personal profile.
                    user.SendMessageW(run[0], 0xF1, 0, 0)  # BM_SETCHECK / BST_UNCHECKED
                    assert user.SendMessageW(run[0], 0xF0, 0, 0) == 0
                    finish = True
                # NSIS creates one inner dialog per page. Text/layout continues
                # settling after Next; using that changing text as a page key
                # would click the same page twice, unlike a user's navigation.
                page = tuple(h for h, _, cls in controls if cls == '#32770')
                if next_button and user.IsWindowEnabled(next_button) and page != last_page:
                    observed.append(label(next_button))
                    last_page = page
                    assert user.PostMessageW(next_button, 0xF5, 0, 0)  # BM_CLICK
            time.sleep(.05)
        assert child.poll() is not None, ('wizard deadline', observed, children(wizard))
        assert answered, ('no native running confirmation', observed)
        if confirm:
            assert child.returncode == 0 and finish, ('repair did not complete', child.returncode, observed)
            assert process_exited(pid), 'confirmed application still running'
        else:
            assert child.returncode != 0, 'cancel reported installation success'
            assert not process_exited(pid), 'cancel terminated application'
        print('PASS: real NSIS running-app ' + ('force close and repair' if confirm else 'cancel, application retained'), flush=True)
    finally:
        if child.poll() is None:
            # Only this Popen-owned installer is terminated on fixture failure.
            # Close its owned modal first so the helper can release its leases.
            if wizard:
                for hwnd, _ in windows():
                    if user.GetWindow(hwnd, 4) == wizard and label(hwnd) == 'Daymark 安装':
                        user.PostMessageW(hwnd, 0x111, 2, 0)
            child.kill()
            child.wait(timeout=5)
