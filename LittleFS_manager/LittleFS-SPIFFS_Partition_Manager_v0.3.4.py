#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from __future__ import annotations

import base64
import sys
import threading
import time
import zipfile
from dataclasses import dataclass, field
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog, ttk

try:
    import serial
    from serial.tools import list_ports
except Exception:
    serial = None
    list_ports = None

APP_VERSION = "0.3.4"
DEFAULT_BAUDRATE = 460800
# Upload tuning:
# 96 bytes is the old proven-safe mode, but it is extremely slow because every chunk
# waits for an ACK. 192 bytes keeps the serial line short enough for typical ESP32
# line parsers while roughly halving the ACK round-trips. If one 192-byte write fails,
# this manager immediately retries the same file with 96 bytes only once.
CHUNK_SIZE = 1024
SAFE_CHUNK_SIZE = 96
FAST_CHUNK_SIZES = (1024, 768, 512, 256, 96)
UPLOAD_INTER_CHUNK_DELAY = 0.0
WRITE_BEGIN_TIMEOUT = 6.0
WRITE_DATA_TIMEOUT_FAST = 1.0
WRITE_DATA_TIMEOUT_SAFE = 2.0
WRITE_END_TIMEOUT = 6.0
MAX_AUTO_RETRIES = 1
DEFAULT_SPIFFS_CAPACITY_KB = 896


TEXT = {
    "HU": {
        "title": "LittleFS-SPIFFS Partition Manager",
        "port": "COM port",
        "refresh_ports": "Portok frissítése",
        "connect": "Kapcsolódás",
        "disconnect": "Kapcsolat bontása",
        "maintenance": "Karbantartó mód indítása",
        "backup": "Teljes mentés (ZIP)",
        "restore": "Mentés visszaállítása",
        "list": "Fájllista frissítése",
        "delete": "Kijelölt törlése",
        "upload_files": "Fájlok várósorba",
        "upload_folder": "Mappa várósorba",
        "download": "Kijelölt mentése",
        "reboot": "Rádió újraindítása",
        "lang": "Nyelv: HU / EN",
        "spiffs_capacity": "FS méret",
        "set_spiffs_capacity": "FS méret beállítása",
        "spiffs_capacity_prompt": "Add meg a teljes fájlrendszer partíció méretet KB-ban.\nÜresen hagyva a helyellenőrzés kikapcsolva.",
        "spiffs_capacity_disabled": "FS helyellenőrzés: kikapcsolva",
        "spiffs_capacity_set": "FS helyellenőrzés aktív: {value} KB",
        "space_check_insufficient": "Kevés a becsült szabad hely.\n\nBecsült szabad hely: {free}\nVárósor mérete: {need}\n\nFeltöltés megszakítva.",
        "space_check_low": "Kevés a becsült szabad hely.\n\nBecsült szabad hely: {free}\nVárósor mérete: {need}\n\nA feltöltés még elindítható, de nagy az open_failed hiba esélye.\nFolytatod?",
        "critical_spiffs_write_error": "Kritikus fájlrendszer írási hiba, a várósor leállítva.",
        "open_failed_hint": "A rádió nem tudta megnyitni a célfájlt írásra. Ez általában kevés vagy töredezett fájlrendszer szabad helyre utal.",
        "tree": "A rádió fájlrendszer tartalma",
        "type": "Típus",
        "size": "Méret",
        "status_ready": "Készen.",
        "status_connecting": "Kapcsolódás folyamatban...",
        "status_maintenance": "Karbantartó mód indítása...",
        "status_listing": "Fájllista frissítése...",
        "status_saving": "Mentés folyamatban...",
        "status_restoring": "Visszaállítás folyamatban...",
        "status_verifying": "Ellenőrzés folyamatban...",
        "status_uploading": "Feltöltés folyamatban...",
        "status_deleting": "Törlés folyamatban...",
        "status_downloading": "Mentés a számítógépre...",
        "status_rebooting": "Újraindítás kérése...",
        "target_folder": "célmappa",
        "mkdir": "Mappa létrehozása",
        "enter_dir_name": "Add meg az új mappa nevét:",
        "mkdir_done": "Mappa sikeresen létrehozva.",
        "status_mkdir": "Mappa létrehozása...",
        "connect_first": "Előbb csatlakozz a rádióhoz.",
        "error": "Hiba",
        "warning": "Figyelmeztetés",
        "done": "Kész",
        "saved": "Mentve",
        "maintenance_ok": "Karbantartó mód aktív.",
        "ports_none": "Nincs találat",
        "pyserial_missing": "A pyserial nincs telepítve.\nParancs: python -m pip install pyserial",
        "tree_no_selection": "Jelölj ki egy fájlt vagy mappát.",
        "backup_done": "A teljes mentés elkészült.",
        "restore_done": "A visszaállítás elkészült.",
        "download_done": "A kijelölt fájl mentése elkészült.",
        "upload_done": "A feltöltés elkészült.",
        "delete_done": "A törlés elkészült.",
        "reboot_done": "Az újraindítás kérése elküldve.",
        "no_files": "Nincs fájl a rádión.",
        "empty_folder": "A kiválasztott mappa üres.",
        "folder": "mappa",
        "file": "fájl",
        "queue": "Feltöltési várósor",
        "queue_name": "Név",
        "queue_target": "Cél",
        "queue_status": "Állapot",
        "queue_progress": "Folyamat",
        "queue_size": "Méret",
        "queue_add_files": "Fájlok hozzáadása",
        "queue_add_folder": "Mappa hozzáadása",
        "queue_start": "Várósor indítása",
        "queue_cancel": "Megszakítás",
        "queue_retry": "Hibásak újra",
        "queue_remove": "Kijelölt eltávolítása",
        "queue_clear_done": "Készek törlése",
        "queue_idle": "A várósor üres.",
        "queue_waiting": "Várakozik",
        "queue_uploading": "Feltöltés",
        "queue_done": "Kész",
        "queue_failed": "Hibás",
        "queue_cancelled": "Megszakítva",
        "queue_retrying": "Újrapróba",
        "queue_running": "A várósor fut.",
        "queue_added": "A fájlok bekerültek a várósorba.",
        "queue_cancel_requested": "Megszakítás kérve...",
        "queue_finished": "Várósor kész.",
        "queue_empty_start": "Nincs feltöltendő elem a várósorban.",
        "queue_file": "Aktuális fájl",
        "queue_overall": "Összesen",
        "queue_speed": "Sebesség",
        "queue_eta": "Hátralévő idő",
        "queue_index": "Fájl",
        "queue_failures": "Hibák",
        "queue_cancelled_done": "A várósor megszakadt.",
        "last_step": "Utolsó művelet",
        "save_selected_title": "Fájl mentése",
        "save_backup_title": "Mentés mentése ZIP fájlba",
        "open_backup_title": "Mentés kiválasztása",
        "footer": "2026 © gidano",
    },
    "EN": {
        "title": "LittleFS-SPIFFS Partition Manager",
        "port": "COM port",
        "refresh_ports": "Refresh ports",
        "connect": "Connect",
        "disconnect": "Disconnect",
        "maintenance": "Start maintenance mode",
        "backup": "Full backup (ZIP)",
        "restore": "Restore backup",
        "list": "Refresh file list",
        "delete": "Delete selected",
        "upload_files": "Add files to queue",
        "upload_folder": "Add folder to queue",
        "download": "Save selected",
        "reboot": "Reboot radio",
        "lang": "Language: HU / EN",
        "spiffs_capacity": "FS size",
        "set_spiffs_capacity": "Set FS size",
        "spiffs_capacity_prompt": "Enter total filesystem partition size in KB.\nLeave empty to disable space checks.",
        "spiffs_capacity_disabled": "FS space check: disabled",
        "spiffs_capacity_set": "FS space check enabled: {value} KB",
        "space_check_insufficient": "Estimated free space is too low.\n\nEstimated free space: {free}\nQueue size: {need}\n\nUpload aborted.",
        "space_check_low": "Estimated free space is low.\n\nEstimated free space: {free}\nQueue size: {need}\n\nUpload can still be started, but open_failed errors are likely.\nContinue?",
        "critical_spiffs_write_error": "Critical filesystem write error, queue stopped.",
        "open_failed_hint": "The radio could not open the target file for writing. This usually points to low or fragmented filesystem free space.",
        "tree": "Radio filesystem contents",
        "type": "Type",
        "size": "Size",
        "status_ready": "Ready.",
        "status_connecting": "Connecting...",
        "status_maintenance": "Starting maintenance mode...",
        "status_listing": "Refreshing file list...",
        "status_saving": "Saving backup...",
        "status_restoring": "Restoring backup...",
        "status_verifying": "Verifying...",
        "status_uploading": "Uploading...",
        "status_deleting": "Deleting...",
        "status_downloading": "Saving to computer...",
        "status_rebooting": "Requesting reboot...",
        "target_folder": "target folder",
        "mkdir": "Create directory",
        "enter_dir_name": "Enter directory name:",
        "mkdir_done": "Directory created successfully.",
        "status_mkdir": "Creating directory...",
        "connect_first": "Connect to the radio first.",
        "error": "Error",
        "warning": "Warning",
        "done": "Done",
        "saved": "Saved",
        "maintenance_ok": "Maintenance mode is active.",
        "ports_none": "No ports found",
        "pyserial_missing": "pyserial is not installed.\nCommand: python -m pip install pyserial",
        "tree_no_selection": "Select a file or folder.",
        "backup_done": "Full backup completed.",
        "restore_done": "Restore completed.",
        "download_done": "Selected file saved.",
        "upload_done": "Upload completed.",
        "delete_done": "Delete completed.",
        "reboot_done": "Reboot requested.",
        "no_files": "There are no files on the radio.",
        "empty_folder": "The selected folder is empty.",
        "folder": "folder",
        "file": "file",
        "queue": "Upload queue",
        "queue_name": "Name",
        "queue_target": "Target",
        "queue_status": "Status",
        "queue_progress": "Progress",
        "queue_size": "Size",
        "queue_add_files": "Add files",
        "queue_add_folder": "Add folder",
        "queue_start": "Start queue",
        "queue_cancel": "Cancel",
        "queue_retry": "Retry failed",
        "queue_remove": "Remove selected",
        "queue_clear_done": "Clear completed",
        "queue_idle": "The queue is empty.",
        "queue_waiting": "Waiting",
        "queue_uploading": "Uploading",
        "queue_done": "Done",
        "queue_failed": "Failed",
        "queue_cancelled": "Cancelled",
        "queue_retrying": "Retrying",
        "queue_running": "The queue is running.",
        "queue_added": "Files added to queue.",
        "queue_cancel_requested": "Cancel requested...",
        "queue_finished": "Queue finished.",
        "queue_empty_start": "There is nothing in the upload queue.",
        "queue_file": "Current file",
        "queue_overall": "Overall",
        "queue_speed": "Speed",
        "queue_eta": "ETA",
        "queue_index": "File",
        "queue_failures": "Failures",
        "queue_cancelled_done": "Queue cancelled.",
        "last_step": "Last step",
        "save_selected_title": "Save file",
        "save_backup_title": "Save backup ZIP",
        "open_backup_title": "Choose backup ZIP",
        "footer": "2026 © gidano",
    },
}


def normalize_remote_path(path: str) -> str:
    path = (path or "").replace("\\", "/")
    while "//" in path:
        path = path.replace("//", "/")
    if not path.startswith("/"):
        path = "/" + path
    return path


def format_eta(seconds: float | None) -> str:
    if seconds is None or seconds < 0 or seconds == float("inf"):
        return "--:--"
    sec = int(seconds)
    m, s = divmod(sec, 60)
    h, m = divmod(m, 60)
    if h:
        return f"{h:d}:{m:02d}:{s:02d}"
    return f"{m:02d}:{s:02d}"


def human_speed(bytes_per_sec: float) -> str:
    if bytes_per_sec <= 0:
        return "0 KB/s"
    kb = bytes_per_sec / 1024.0
    if kb < 1024:
        return f"{kb:.1f} KB/s"
    return f"{kb / 1024.0:.2f} MB/s"


def fmt_size(size: int) -> str:
    value = float(size)
    for unit in ("B", "KB", "MB"):
        if value < 1024 or unit == "MB":
            if unit == "B":
                return f"{int(value)} {unit}"
            return f"{value:.1f} {unit}"
        value /= 1024
    return f"{size} B"




def parse_positive_int_or_none(value: str) -> int | None:
    value = (value or "").strip()
    if not value:
        return None
    digits = "".join(ch for ch in value if ch.isdigit())
    if not digits:
        return None
    number = int(digits)
    return number if number > 0 else None


def is_probable_spiffs_open_failed(error_text: str) -> bool:
    msg = (error_text or "").lower()
    return "open_failed" in msg or ("write_begin" in msg and "open" in msg and "failed" in msg)


def build_open_failed_hint(base_error: str, localized_hint: str) -> str:
    base_error = (base_error or "").strip()
    if not base_error:
        return localized_hint
    if localized_hint in base_error:
        return base_error
    return f"{base_error} | {localized_hint}"


def set_windows_app_id():
    try:
        import ctypes
        ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID("gidano.myRadio.FS.Kezelo")
    except Exception:
        pass


def get_app_icon_path() -> str | None:
    candidates = []
    meipass = getattr(sys, "_MEIPASS", None)
    if meipass:
        candidates.append(Path(meipass) / "icon.ico")
    try:
        candidates.append(Path(sys.executable).resolve().parent / "icon.ico")
    except Exception:
        pass
    try:
        candidates.append(Path(__file__).resolve().parent / "icon.ico")
    except Exception:
        pass
    for p in candidates:
        if p.exists():
            return str(p)
    return None


def is_windows_dark_mode():
    try:
        import winreg
        key = winreg.OpenKey(
            winreg.HKEY_CURRENT_USER,
            r"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize"
        )
        value, _ = winreg.QueryValueEx(key, "AppsUseLightTheme")
        return value == 0
    except Exception:
        return False


def apply_dark_title_bar(window):
    try:
        import ctypes
        hwnd = ctypes.windll.user32.GetParent(window.winfo_id())
        value = ctypes.c_int(1)
        for attr in (20, 19):
            try:
                ctypes.windll.dwmapi.DwmSetWindowAttribute(hwnd, attr, ctypes.byref(value), ctypes.sizeof(value))
            except Exception:
                pass
    except Exception:
        pass


def apply_theme(root: tk.Tk, dark: bool):
    style = ttk.Style(root)
    if dark:
        try:
            style.theme_use("clam")
        except Exception:
            pass
        bg = "#1e1e1e"
        panel = "#252526"
        fg = "#ffffff"
        edge = "#6f6f6f"
        select = "#3a3d41"
        root.configure(bg=bg)
        style.configure(".", background=bg, foreground=fg)
        style.configure("TFrame", background=bg)
        style.configure("TLabelframe", background=bg, foreground=fg)
        style.configure("TLabelframe.Label", background=bg, foreground=fg)
        style.configure("TLabel", background=bg, foreground=fg)
        style.configure("TButton", background=panel, foreground=fg, bordercolor=edge, focusthickness=1, focuscolor=edge)
        style.map("TButton", background=[("active", "#2f3136"), ("pressed", "#2a2d31")], foreground=[("disabled", "#8a8a8a")])
        style.configure("TCombobox", fieldbackground=panel, background=panel, foreground=fg, arrowcolor=fg, bordercolor=edge, lightcolor=edge, darkcolor=edge)
        style.map("TCombobox", fieldbackground=[("readonly", panel)], selectbackground=[("readonly", select)], selectforeground=[("readonly", fg)])
        style.configure("Treeview", background=panel, fieldbackground=panel, foreground=fg, bordercolor=edge, lightcolor=edge, darkcolor=edge)
        style.configure("Treeview.Heading", background="#2d2d30", foreground=fg, bordercolor=edge, lightcolor=edge, darkcolor=edge)
        style.map("Treeview", background=[("selected", select)], foreground=[("selected", fg)])
        style.map("Treeview.Heading", background=[("active", "#383b40")])
        style.configure("Horizontal.TProgressbar", troughcolor=panel, background="#6aa2ff", bordercolor=edge, lightcolor=edge, darkcolor=edge)
        style.configure("Vertical.TScrollbar", background=panel, troughcolor=bg, arrowcolor=fg, bordercolor=edge, lightcolor=edge, darkcolor=edge)
        style.configure("Horizontal.TScrollbar", background=panel, troughcolor=bg, arrowcolor=fg, bordercolor=edge, lightcolor=edge, darkcolor=edge)
    else:
        try:
            style.theme_use("vista")
        except Exception:
            pass


class ProtoError(RuntimeError):
    pass


@dataclass
class RemoteFile:
    path: str
    size: int
    is_dir: bool = False


@dataclass
class UploadTask:
    local_path: Path
    remote_path: str
    size: int
    status: str = "waiting"
    progress_pct: float = 0.0
    uploaded_bytes: int = 0
    retries_done: int = 0
    max_retries: int = MAX_AUTO_RETRIES
    error: str = ""
    task_id: str = field(default_factory=lambda: f"task-{time.time_ns()}")


class SerialSpiFFSClient:
    def __init__(self):
        self.ser = None
        self.debug_lines: list[str] = []

    def connect(self, port: str, baudrate: int = DEFAULT_BAUDRATE, timeout: float = 0.8):
        if serial is None:
            raise ProtoError("pyserial not installed")

        tmp = serial.Serial()
        tmp.port = port
        tmp.baudrate = baudrate
        tmp.timeout = timeout
        tmp.write_timeout = 20
        try:
            tmp.dtr = False
            tmp.rts = False
        except Exception:
            pass
        tmp.open()
        self.ser = tmp

        time.sleep(1.7)
        self.light_drain(0.6)

    def disconnect(self):
        if self.ser:
            try:
                self.ser.close()
            finally:
                self.ser = None

    def light_drain(self, duration: float = 0.25):
        if not self.ser:
            return
        end = time.time() + duration
        while time.time() < end:
            raw = self.ser.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="ignore").strip()
            if line:
                self.debug_lines.append(line)
                self.debug_lines = self.debug_lines[-20:]

    def clear_input(self):
        if not self.ser:
            return
        try:
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
        except Exception:
            pass
        self.light_drain(0.15)

    def _write_line(self, line: str, *, debug: bool = True):
        if not self.ser:
            raise ProtoError("not connected")
        self.ser.write((line + "\n").encode("utf-8"))

        # WRITE_DATA is sent thousands of times during uploads.
        # Avoid per-chunk flush() because it massively reduces throughput
        # on many USB serial drivers.
        if not line.startswith("WRITE_DATA|"):
            self.ser.flush()

        if debug:
            self.debug_lines.append(f">>> {line}")
            self.debug_lines = self.debug_lines[-20:]

    def _read_proto_line(self, timeout: float = 15.0) -> str:
        if not self.ser:
            raise ProtoError("not connected")
        end = time.time() + timeout
        last_noise = ""
        buf = bytearray()
        while time.time() < end:
            raw = self.ser.read(1)
            if not raw:
                continue
            ch = raw[0]
            if ch == 13:
                continue
            if ch != 10:
                buf.append(ch)
                if len(buf) > 4096:
                    last_noise = buf[:120].decode("utf-8", errors="ignore")
                    buf.clear()
                continue

            if not buf:
                continue
            line = buf.decode("utf-8", errors="ignore").strip()
            buf.clear()
            if not line:
                continue
            self.debug_lines.append(line)
            self.debug_lines = self.debug_lines[-20:]
            if not line.startswith("MRSPIFS|"):
                last_noise = line
                continue
            return line[len("MRSPIFS|"):]
        if last_noise:
            raise ProtoError(f"protocol timeout (last serial: {last_noise[:120]})")
        if buf:
            tail = buf[:120].decode("utf-8", errors="ignore")
            raise ProtoError(f"protocol timeout (partial serial line: {tail})")
        raise ProtoError("protocol timeout")

    def _hello_begin_handshake(self, window: float) -> bool:
        """LittleFS-manager compatible HELLO -> BEGIN boot-window handshake."""
        deadline = time.time() + window
        while time.time() < deadline:
            self._write_line("HELLO")
            try:
                line = self._read_proto_line(timeout=1.2)
            except ProtoError:
                continue
            parts = line.split("|")
            if not (len(parts) >= 2 and parts[0] == "OK" and parts[1] == "HELLO"):
                continue
            self._write_line("BEGIN")
            try:
                line = self._read_proto_line(timeout=2.0)
            except ProtoError:
                continue
            parts = line.split("|")
            if len(parts) >= 2 and parts[0] == "OK" and parts[1] == "BEGIN":
                return True
        return False

    def begin_maintenance(self):
        # Newer LittleFS-capable firmware may expose a clean reboot-maintenance window.
        # Keep the old MRSPIFS:BEGIN fallback, so existing SPIFFS firmware still works.
        self.clear_input()
        try:
            self._write_line("REBOOT_MAINT")
            try:
                self._read_proto_line(timeout=2.0)
            except ProtoError:
                pass
            time.sleep(2.5)
            self.clear_input()
            if self._hello_begin_handshake(window=6.0):
                return True
        except Exception:
            self.clear_input()

        for _ in range(14):
            self._write_line("MRSPIFS:BEGIN")
            try:
                line = self._read_proto_line(timeout=2.8)
            except ProtoError:
                time.sleep(0.25)
                continue
            parts = line.split("|")
            if len(parts) >= 2 and parts[0] == "OK" and parts[1] == "BEGIN":
                return True
            raise ProtoError("unexpected BEGIN reply: " + line)
        raise ProtoError("could not enter maintenance mode")

    def ping(self):
        self._write_line("PING")
        parts = self._read_proto_line(timeout=5.0).split("|")
        if len(parts) >= 2 and parts[0] == "OK" and parts[1] == "PING":
            return True
        raise ProtoError("bad PING reply")

    def abort_write(self):
        self._write_line("WRITE_ABORT")
        parts = self._read_proto_line(timeout=5.0).split("|")
        if not (len(parts) >= 2 and parts[0] == "OK" and parts[1] == "WRITE_ABORT"):
            raise ProtoError("bad WRITE_ABORT reply: " + "|".join(parts))

    def ensure_idle(self):
        try:
            self.abort_write()
        except Exception:
            self.clear_input()

    def list_files(self) -> list[RemoteFile]:
        self._write_line("LIST")
        files = []
        while True:
            parts = self._read_proto_line(timeout=20.0).split("|")
            if not parts:
                continue
            if parts[0] == "FILE" and len(parts) >= 3:
                try:
                    files.append(RemoteFile(normalize_remote_path(parts[1]), int(parts[2]), False))
                except ValueError:
                    pass
            elif parts[0] == "DIR" and len(parts) >= 2:
                files.append(RemoteFile(normalize_remote_path(parts[1]), 0, True))
            elif parts[0] == "OK" and len(parts) >= 2 and parts[1] == "LIST":
                break
            elif parts[0] == "ERR":
                raise ProtoError("|".join(parts))
        return sorted(files, key=lambda x: (x.path.lower(), not x.is_dir))

    def read_file(self, path: str) -> bytes:
        path = normalize_remote_path(path)
        self._write_line(f"READ|PATH|{path}")
        out = bytearray()
        while True:
            parts = self._read_proto_line(timeout=20.0).split("|")
            if not parts:
                continue
            if parts[0] == "READ_BEGIN":
                continue
            if parts[0] == "DATA" and len(parts) >= 2:
                b64 = parts[1].strip()
                if len(b64) % 4 != 0:
                    raise ProtoError(f"corrupt READ data for {path}: base64 length {len(b64)} is not divisible by 4")
                try:
                    out.extend(base64.b64decode(b64, validate=True))
                except Exception as e:
                    raise ProtoError(f"corrupt READ data for {path}: {e}") from e
                continue
            if parts[0] == "OK" and len(parts) >= 2 and parts[1] == "READ_END":
                return bytes(out)
            if parts[0] == "ERR":
                raise ProtoError("|".join(parts))

    def write_file(self, path: str, data: bytes):
        path = normalize_remote_path(path)
        last_err = None

        # Safe-fast upload:
        # - first try 192-byte chunks for roughly 2x fewer ACK round-trips than 96
        # - if the firmware rejects/times out, abort immediately and retry at 96
        # - no oversized 512/768/1024 tests, so small files do not sit for minutes
        for attempt, chunk_size in enumerate(FAST_CHUNK_SIZES, 1):
            timeout = WRITE_DATA_TIMEOUT_SAFE if chunk_size <= SAFE_CHUNK_SIZE else WRITE_DATA_TIMEOUT_FAST
            try:
                self.ensure_idle()
                self._write_line(f"WRITE_BEGIN|PATH|{path}|{len(data)}")
                parts = self._read_proto_line(timeout=WRITE_BEGIN_TIMEOUT).split("|")
                if not (len(parts) >= 2 and parts[0] == "OK" and parts[1] == "WRITE_BEGIN"):
                    raise ProtoError("bad WRITE_BEGIN reply: " + "|".join(parts))

                total_chunks = max(1, (len(data) + chunk_size - 1) // chunk_size)
                uploaded = 0

                if not data:
                    yield 1, 1, 0

                for idx, i in enumerate(range(0, len(data), chunk_size), 1):
                    chunk = data[i:i + chunk_size]
                    b64 = base64.b64encode(chunk).decode("ascii")
                    self._write_line(f"WRITE_DATA|B64|{b64}", debug=False)
                    parts = self._read_proto_line(timeout=timeout).split("|")
                    if not (len(parts) >= 2 and parts[0] == "OK" and parts[1] == "WRITE_DATA"):
                        raise ProtoError(
                            f"bad WRITE_DATA reply at chunk {idx}/{total_chunks} "
                            f"for {path} with chunk_size={chunk_size}: " + "|".join(parts)
                        )
                    uploaded += len(chunk)
                    self.debug_lines.append(f"UPLOAD_CHUNK={chunk_size}")
                    self.debug_lines = self.debug_lines[-20:]
                    yield idx, total_chunks, uploaded
                    if UPLOAD_INTER_CHUNK_DELAY:
                        time.sleep(UPLOAD_INTER_CHUNK_DELAY)

                self._write_line("WRITE_END")
                parts = self._read_proto_line(timeout=WRITE_END_TIMEOUT).split("|")
                if not (len(parts) >= 2 and parts[0] == "OK" and parts[1] == "WRITE_END"):
                    raise ProtoError("bad WRITE_END reply: " + "|".join(parts))
                return

            except Exception as e:
                last_err = e
                try:
                    self.abort_write()
                except Exception:
                    self.clear_input()

                # Do not keep retrying failed fast mode. One quick fallback is enough.
                if chunk_size <= SAFE_CHUNK_SIZE:
                    break
                time.sleep(0.05)

        raise ProtoError(f"write failed after safe fallback for {path}: {last_err}")

    def delete_file(self, path: str):
        path = normalize_remote_path(path)
        self._write_line(f"DELETE|PATH|{path}")
        parts = self._read_proto_line(timeout=15.0).split("|")
        if not (len(parts) >= 2 and parts[0] == "OK" and parts[1] == "DELETE"):
            raise ProtoError("bad DELETE reply: " + "|".join(parts))

    def mkdir(self, path: str):
        path = normalize_remote_path(path)
        self._write_line(f"MKDIR|PATH|{path}")
        parts = self._read_proto_line(timeout=15.0).split("|")
        if not (len(parts) >= 2 and parts[0] == "OK" and parts[1] == "MKDIR"):
            raise ProtoError("bad MKDIR reply: " + "|".join(parts))

    def rmdir(self, path: str):
        path = normalize_remote_path(path)
        self._write_line(f"RMDIR|PATH|{path}")
        parts = self._read_proto_line(timeout=15.0).split("|")
        if not (len(parts) >= 2 and parts[0] == "OK" and parts[1] == "RMDIR"):
            raise ProtoError("bad RMDIR reply: " + "|".join(parts))

    def reboot(self):
        self._write_line("REBOOT")
        parts = self._read_proto_line(timeout=5.0).split("|")
        if not (len(parts) >= 2 and parts[0] == "OK" and parts[1] == "REBOOT"):
            raise ProtoError("bad REBOOT reply: " + "|".join(parts))


class App(tk.Tk):
    def __init__(self):
        set_windows_app_id()
        super().__init__()
        self.lang = "HU"
        self.client = SerialSpiFFSClient()
        self.files: list[RemoteFile] = []
        self.worker = None
        self.cancel_event = threading.Event()
        self.queue_lock = threading.Lock()
        self.upload_queue: list[UploadTask] = []
        self.queue_running = False
        self.current_queue_task_id: str | None = None
        self.known_remote_dirs: set[str] = {"/"}
        # Treeview item metadata.  Do not reconstruct remote paths only from
        # visible labels, because virtual folders are synthesized from file paths
        # when the firmware does not report DIR entries explicitly.
        self.tree_item_info: dict[str, tuple[str, bool]] = {}
        # Remember full paths uploaded/created by this tool. This repairs GUI display
        # when the firmware LIST command later reports only basename-only root files.
        self.known_remote_file_paths: dict[tuple[str, int], str] = {}
        self.known_remote_dir_paths: set[str] = {"/"}
        self.queue_stop_reason: str | None = None
        self.spiffs_capacity_kb = DEFAULT_SPIFFS_CAPACITY_KB

        self.title(f"{self.tr('title')} v{APP_VERSION}")
        self.geometry("1180x860")
        self.minsize(1180, 720)
        self._dark_mode = is_windows_dark_mode()
        apply_theme(self, self._dark_mode)
        icon_path = get_app_icon_path()
        if icon_path:
            try:
                self.iconbitmap(default=icon_path)
            except Exception:
                try:
                    self.iconbitmap(icon_path)
                except Exception:
                    pass

        self.port_var = tk.StringVar()
        self.status_var = tk.StringVar(value=self.tr("status_ready"))
        self.progress_var = tk.DoubleVar(value=0.0)
        self.overall_progress_var = tk.DoubleVar(value=0.0)
        self.current_file_var = tk.StringVar(value="-")
        self.overall_var = tk.StringVar(value="0 / 0")
        self.speed_var = tk.StringVar(value="0 KB/s")
        self.eta_var = tk.StringVar(value="--:--")
        self.failures_var = tk.StringVar(value="0")

        self._build_ui()
        self.refresh_ports()
        self._tree_scrollbar_after_id = None
        self._queue_tree_scrollbar_after_id = None
        self.after(100, self._refresh_tree_scrollbar)
        self.after(100, self._refresh_queue_tree_scrollbar)
        self.bind("<Configure>", lambda event: self.after_idle(self._on_window_layout_change))
        if self._dark_mode:
            self.after(50, lambda: apply_dark_title_bar(self))
        if serial is None:
            messagebox.showwarning(self.tr("error"), self.tr("pyserial_missing"))

    def tr(self, key: str) -> str:
        return TEXT[self.lang][key]

    def _build_ui(self):
        top = ttk.Frame(self, padding=8)
        top.pack(fill="x")
        self.lbl_port = ttk.Label(top, text=self.tr("port"))
        self.lbl_port.grid(row=0, column=0, sticky="w")
        self.port_combo = ttk.Combobox(top, textvariable=self.port_var, width=22, state="readonly")
        self.port_combo.grid(row=0, column=1, sticky="ew", padx=6)
        self.btn_ports = ttk.Button(top, text=self.tr("refresh_ports"), command=self.refresh_ports)
        self.btn_ports.grid(row=0, column=2, padx=4)
        self.btn_maint = ttk.Button(top, text=self.tr("maintenance"), command=self.connect)
        self.btn_maint.grid(row=0, column=3, padx=4)
        self.btn_lang = ttk.Button(top, text=self.tr("lang"), command=self.toggle_lang)
        self.btn_lang.grid(row=0, column=6, padx=4)
        self.btn_capacity = ttk.Button(top, text=self.tr("spiffs_capacity"), command=self.set_spiffs_capacity)
        self.btn_capacity.grid(row=0, column=7, padx=4)
        top.columnconfigure(1, weight=1)

        actions = ttk.Frame(self, padding=(8, 0, 8, 8))
        actions.pack(fill="x")
        self.btn_list = ttk.Button(actions, text=self.tr("list"), command=self.refresh_list)
        self.btn_list.pack(side="left", padx=3)
        self.btn_backup = ttk.Button(actions, text=self.tr("backup"), command=self.backup_zip)
        self.btn_backup.pack(side="left", padx=3)
        self.btn_restore = ttk.Button(actions, text=self.tr("restore"), command=self.restore_zip)
        self.btn_restore.pack(side="left", padx=3)
        self.btn_download = ttk.Button(actions, text=self.tr("download"), command=self.download_selected)
        self.btn_download.pack(side="left", padx=3)
        self.btn_mkdir = ttk.Button(actions, text=self.tr("mkdir"), command=self.create_directory)
        self.btn_mkdir.pack(side="left", padx=3)
        self.btn_delete = ttk.Button(actions, text=self.tr("delete"), command=self.delete_selected)
        self.btn_delete.pack(side="left", padx=3)
        self.btn_reboot = ttk.Button(actions, text=self.tr("reboot"), command=self.reboot_radio)
        self.btn_reboot.pack(side="left", padx=3)

        self.main_pane = ttk.Panedwindow(self, orient="horizontal")
        self.main_pane.pack(fill="both", expand=True, padx=8, pady=(0, 8))

        left = ttk.Frame(self.main_pane)
        right = ttk.Frame(self.main_pane)
        self.main_pane.add(left, weight=1)
        self.main_pane.add(right, weight=1)

        self.left_panel = ttk.LabelFrame(left, text=self.tr("tree"), padding=8)
        self.left_panel.pack(fill="both", expand=True)
        self.tree_wrap = ttk.Frame(self.left_panel)
        self.tree_wrap.pack(fill="both", expand=True)
        self.tree_wrap.rowconfigure(0, weight=1)
        self.tree_wrap.columnconfigure(0, weight=1)
        self.tree = ttk.Treeview(self.tree_wrap, columns=("type", "size"), show="tree headings", selectmode="extended")
        self.tree.heading("#0", text=self.tr("tree"))
        self.tree.heading("type", text=self.tr("type"))
        self.tree.heading("size", text=self.tr("size"))
        self.tree.column("#0", width=430, minwidth=160, anchor="w", stretch=True)
        self.tree.column("type", width=95, minwidth=90, anchor="w", stretch=False)
        self.tree.column("size", width=85, minwidth=80, anchor="w", stretch=False)
        self.tree_ys = ttk.Scrollbar(self.tree_wrap, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=self._on_tree_yview)
        self.tree.grid(row=0, column=0, sticky="nsew")
        self.tree_ys.grid(row=0, column=1, sticky="ns")
        self.tree_ys.grid_remove()
        self.tree.bind("<<TreeviewOpen>>", lambda event: self._schedule_tree_scrollbar_refresh())
        self.tree.bind("<<TreeviewClose>>", lambda event: self._schedule_tree_scrollbar_refresh())
        self.tree.bind("<Configure>", lambda event: self._schedule_tree_scrollbar_refresh())

        self.queue_panel = ttk.LabelFrame(right, text=self.tr("queue"), padding=8)
        self.queue_panel.pack(fill="both", expand=True)

        queue_buttons = ttk.Frame(self.queue_panel)
        queue_buttons.pack(fill="x", pady=(0, 8))
        self.btn_queue_add_files = ttk.Button(queue_buttons, text=self.tr("queue_add_files"), command=self.queue_add_files)
        self.btn_queue_add_files.pack(side="left", padx=2)
        self.btn_queue_add_folder = ttk.Button(queue_buttons, text=self.tr("queue_add_folder"), command=self.queue_add_folder)
        self.btn_queue_add_folder.pack(side="left", padx=2)
        self.btn_queue_start = ttk.Button(queue_buttons, text=self.tr("queue_start"), command=self.start_queue)
        self.btn_queue_start.pack(side="left", padx=2)
        self.btn_queue_cancel = ttk.Button(queue_buttons, text=self.tr("queue_cancel"), command=self.cancel_queue)
        self.btn_queue_cancel.pack(side="left", padx=2)
        self.btn_queue_remove = ttk.Button(queue_buttons, text=self.tr("queue_remove"), command=self.remove_selected_tasks)
        self.btn_queue_remove.pack(side="left", padx=2)

        self.queue_tree_wrap = ttk.Frame(self.queue_panel)
        self.queue_tree_wrap.pack(fill="both", expand=True)
        self.queue_tree_wrap.rowconfigure(0, weight=1)
        self.queue_tree_wrap.columnconfigure(0, weight=1)

        self.queue_tree = ttk.Treeview(self.queue_tree_wrap, columns=("target", "status", "progress", "size"), show="tree headings", height=18, selectmode="extended")
        self.queue_tree.heading("#0", text=self.tr("queue_name"))
        self.queue_tree.heading("target", text=self.tr("queue_target"))
        self.queue_tree.heading("status", text=self.tr("queue_status"))
        self.queue_tree.heading("progress", text=self.tr("queue_progress"))
        self.queue_tree.heading("size", text=self.tr("queue_size"))
        self.queue_tree.column("#0", width=150, minwidth=120, anchor="w", stretch=True)
        self.queue_tree.column("target", width=130, minwidth=120, anchor="w", stretch=True)
        self.queue_tree.column("status", width=70, minwidth=90, anchor="w", stretch=True)
        self.queue_tree.column("progress", width=60, minwidth=90, anchor="w", stretch=True)
        self.queue_tree.column("size", width=60, minwidth=80, anchor="w", stretch=True)
        self.queue_tree_ys = ttk.Scrollbar(self.queue_tree_wrap, orient="vertical", command=self.queue_tree.yview)
        self.queue_tree.configure(yscrollcommand=self._on_queue_tree_yview)
        self.queue_tree.grid(row=0, column=0, sticky="nsew")
        self.queue_tree_ys.grid(row=0, column=1, sticky="ns")
        self.queue_tree_ys.grid_remove()
        self.queue_tree.bind("<Configure>", lambda event: self._schedule_queue_tree_scrollbar_refresh())

        self.progress_box = ttk.LabelFrame(self.queue_panel, text=self.tr("queue_progress"), padding=8)
        self.progress_box.pack(fill="x", pady=(8, 0))
        self.progress = ttk.Progressbar(self.progress_box, variable=self.progress_var, maximum=100.0)
        self.progress.pack(fill="x")
        self.progress_overall = ttk.Progressbar(self.progress_box, variable=self.overall_progress_var, maximum=100.0)
        self.progress_overall.pack(fill="x", pady=(6, 0))

        grid = ttk.Frame(self.progress_box)
        grid.pack(fill="x", pady=(8, 0))
        for idx in range(0, 4):
            grid.columnconfigure(idx, weight=1)
        self.lbl_queue_file = ttk.Label(grid, text=self.tr("queue_file"))
        self.lbl_queue_file.grid(row=0, column=0, sticky="w")
        ttk.Label(grid, textvariable=self.current_file_var).grid(row=0, column=1, sticky="w")
        self.lbl_queue_index = ttk.Label(grid, text=self.tr("queue_index"))
        self.lbl_queue_index.grid(row=0, column=2, sticky="w")
        ttk.Label(grid, textvariable=self.overall_var).grid(row=0, column=3, sticky="w")
        self.lbl_queue_speed = ttk.Label(grid, text=self.tr("queue_speed"))
        self.lbl_queue_speed.grid(row=1, column=0, sticky="w", pady=(4, 0))
        ttk.Label(grid, textvariable=self.speed_var).grid(row=1, column=1, sticky="w", pady=(4, 0))
        self.lbl_queue_eta = ttk.Label(grid, text=self.tr("queue_eta"))
        self.lbl_queue_eta.grid(row=1, column=2, sticky="w", pady=(4, 0))
        ttk.Label(grid, textvariable=self.eta_var).grid(row=1, column=3, sticky="w", pady=(4, 0))
        self.lbl_queue_failures = ttk.Label(grid, text=self.tr("queue_failures"))
        self.lbl_queue_failures.grid(row=2, column=0, sticky="w", pady=(4, 0))
        ttk.Label(grid, textvariable=self.failures_var).grid(row=2, column=1, sticky="w", pady=(4, 0))
        self.lbl_queue_overall = ttk.Label(grid, text=self.tr("queue_overall"))
        self.lbl_queue_overall.grid(row=2, column=2, sticky="w", pady=(4, 0))
        self.queue_status_label = ttk.Label(grid, textvariable=self.status_var, wraplength=320, justify="left")
        self.queue_status_label.grid(row=2, column=3, sticky="ew", pady=(4, 0))

        self.after(100, self._apply_initial_layout)

        bottom = ttk.Frame(self, padding=(8, 0, 8, 8))
        bottom.pack(fill="x")
        ttk.Label(bottom, text=self.tr("footer")).pack(side="right")

    def _schedule_tree_scrollbar_refresh(self):
        try:
            if self._tree_scrollbar_after_id is not None:
                self.after_cancel(self._tree_scrollbar_after_id)
        except Exception:
            pass
        self._tree_scrollbar_after_id = self.after_idle(self._refresh_tree_scrollbar)

    def _schedule_queue_tree_scrollbar_refresh(self):
        try:
            if self._queue_tree_scrollbar_after_id is not None:
                self.after_cancel(self._queue_tree_scrollbar_after_id)
        except Exception:
            pass
        self._queue_tree_scrollbar_after_id = self.after_idle(self._refresh_queue_tree_scrollbar)

    def _set_queue_tree_scrollbar_visible(self, visible: bool):
        try:
            if visible:
                self.queue_tree_ys.grid()
            else:
                self.queue_tree_ys.grid_remove()
        except Exception:
            pass

    def _on_queue_tree_yview(self, first, last):
        self.queue_tree_ys.set(first, last)
        try:
            need_scroll = not (float(first) <= 0.0 and float(last) >= 1.0)
        except Exception:
            need_scroll = True
        self._set_queue_tree_scrollbar_visible(need_scroll)

    def _refresh_queue_tree_scrollbar(self):
        self._queue_tree_scrollbar_after_id = None
        try:
            self.update_idletasks()
            first, last = self.queue_tree.yview()
            need_scroll = not (float(first) <= 0.0 and float(last) >= 1.0)
            self._set_queue_tree_scrollbar_visible(need_scroll)
            if need_scroll:
                self.queue_tree_ys.set(first, last)
        except Exception:
            self._set_queue_tree_scrollbar_visible(False)

    def _set_tree_scrollbar_visible(self, visible: bool):
        try:
            if visible:
                self.tree_ys.grid()
            else:
                self.tree_ys.grid_remove()
        except Exception:
            pass

    def _on_tree_yview(self, first, last):
        self.tree_ys.set(first, last)
        try:
            need_scroll = not (float(first) <= 0.0 and float(last) >= 1.0)
        except Exception:
            need_scroll = True
        self._set_tree_scrollbar_visible(need_scroll)

    def _refresh_tree_scrollbar(self):
        self._tree_scrollbar_after_id = None
        try:
            self.update_idletasks()
            first, last = self.tree.yview()
            need_scroll = not (float(first) <= 0.0 and float(last) >= 1.0)
            self._set_tree_scrollbar_visible(need_scroll)
            if need_scroll:
                self.tree_ys.set(first, last)
        except Exception:
            self._set_tree_scrollbar_visible(False)

    def _resize_left_tree_columns(self):
        try:
            total = self.tree.winfo_width()
            if total <= 120:
                return
            type_w = max(90, min(130, int(total * 0.18)))
            size_w = max(80, min(120, int(total * 0.16)))
            name_w = max(160, total - type_w - size_w - 6)
            self.tree.column("#0", width=name_w, minwidth=160, stretch=True)
            self.tree.column("type", width=type_w, minwidth=90, stretch=False)
            self.tree.column("size", width=size_w, minwidth=80, stretch=False)
        except Exception:
            pass

    def _resize_queue_tree_columns(self):
        try:
            total = self.queue_tree.winfo_width()
            if total <= 160:
                return
            name_w = max(120, int(total * 0.26))
            target_w = max(150, int(total * 0.36))
            status_w = max(90, int(total * 0.14))
            progress_w = max(90, int(total * 0.12))
            size_w = max(80, total - name_w - target_w - status_w - progress_w - 6)
            self.queue_tree.column("#0", width=name_w, minwidth=120, stretch=True)
            self.queue_tree.column("target", width=target_w, minwidth=150, stretch=True)
            self.queue_tree.column("status", width=status_w, minwidth=90, stretch=False)
            self.queue_tree.column("progress", width=progress_w, minwidth=90, stretch=False)
            self.queue_tree.column("size", width=size_w, minwidth=80, stretch=False)
            self._schedule_queue_tree_scrollbar_refresh()
        except Exception:
            pass

    def _update_status_wrap(self):
        try:
            wrap = max(180, self.progress_box.winfo_width() - 340)
            self.queue_status_label.configure(wraplength=wrap)
        except Exception:
            pass

    def _on_window_layout_change(self):
        self._schedule_tree_scrollbar_refresh()
        self._schedule_queue_tree_scrollbar_refresh()
        self._resize_left_tree_columns()
        self._resize_queue_tree_columns()
        self._update_status_wrap()

    def _apply_initial_layout(self):
        try:
            total = self.main_pane.winfo_width()
            if total > 0:
                self.main_pane.sashpos(0, int(total * 0.48))
        except Exception:
            pass
        self.after_idle(self._on_window_layout_change)


    def set_spiffs_capacity(self):
        initial = "" if self.spiffs_capacity_kb is None else str(self.spiffs_capacity_kb)
        value = simpledialog.askstring(
            self.tr("set_spiffs_capacity"),
            self.tr("spiffs_capacity_prompt"),
            initialvalue=initial,
            parent=self,
        )
        if value is None:
            return
        parsed = parse_positive_int_or_none(value)
        self.spiffs_capacity_kb = parsed
        if parsed is None:
            self.set_status(self.tr("spiffs_capacity_disabled"))
        else:
            self.set_status(self.tr("spiffs_capacity_set").format(value=parsed))

    def toggle_lang(self):
        self.lang = "EN" if self.lang == "HU" else "HU"
        self.title(f"{self.tr('title')} v{APP_VERSION}")
        self.lbl_port.config(text=self.tr("port"))
        self.btn_ports.config(text=self.tr("refresh_ports"))
        self.btn_maint.config(text=self.tr("maintenance"))
        self.btn_lang.config(text=self.tr("lang"))
        self.btn_capacity.config(text=self.tr("spiffs_capacity"))
        self.btn_list.config(text=self.tr("list"))
        self.btn_backup.config(text=self.tr("backup"))
        self.btn_restore.config(text=self.tr("restore"))
        self.btn_download.config(text=self.tr("download"))
        self.btn_mkdir.config(text=self.tr("mkdir"))
        self.btn_delete.config(text=self.tr("delete"))
        self.btn_reboot.config(text=self.tr("reboot"))
        self.btn_queue_add_files.config(text=self.tr("queue_add_files"))
        self.btn_queue_add_folder.config(text=self.tr("queue_add_folder"))
        self.btn_queue_start.config(text=self.tr("queue_start"))
        self.btn_queue_cancel.config(text=self.tr("queue_cancel"))
        self.btn_queue_remove.config(text=self.tr("queue_remove"))
        self.tree.heading("#0", text=self.tr("tree"))
        self.tree.heading("type", text=self.tr("type"))
        self.tree.heading("size", text=self.tr("size"))
        self.left_panel.config(text=self.tr("tree"))
        self.queue_panel.config(text=self.tr("queue"))
        self.progress_box.config(text=self.tr("queue_progress"))
        self.lbl_queue_file.config(text=self.tr("queue_file"))
        self.lbl_queue_index.config(text=self.tr("queue_index"))
        self.lbl_queue_speed.config(text=self.tr("queue_speed"))
        self.lbl_queue_eta.config(text=self.tr("queue_eta"))
        self.lbl_queue_failures.config(text=self.tr("queue_failures"))
        self.lbl_queue_overall.config(text=self.tr("queue_overall"))
        self.queue_tree.heading("#0", text=self.tr("queue_name"))
        self.queue_tree.heading("target", text=self.tr("queue_target"))
        self.queue_tree.heading("status", text=self.tr("queue_status"))
        self.queue_tree.heading("progress", text=self.tr("queue_progress"))
        self.queue_tree.heading("size", text=self.tr("queue_size"))
        current_status = self.status_var.get()
        for key in ("status_ready", "maintenance_ok", "queue_idle", "queue_running", "queue_finished", "queue_cancelled_done"):
            other = TEXT["EN" if self.lang == "HU" else "HU"][key]
            if current_status == other or current_status == TEXT[self.lang][key]:
                self.status_var.set(self.tr(key))
                break
        apply_theme(self, self._dark_mode)
        self.refresh_queue_tree()
        self.after_idle(self._on_window_layout_change)

    def set_status(self, text: str):
        self.after(0, lambda: self.status_var.set(text))

    def _localize_error(self, text: str) -> str:
        if self.lang != "HU":
            return text
        replacements = {
            "could not enter maintenance mode": "Nem sikerült belépni a karbantartó módba",
            "unexpected BEGIN reply": "Váratlan BEGIN válasz",
            "protocol timeout": "Kommunikációs időtúllépés",
            "not connected": "Nincs kapcsolat",
            "bad DELETE reply": "Hibás törlési válasz",
            "delete verification failed": "A törlés ellenőrzése sikertelen",
            "bad RMDIR reply": "Hibás mappatörlési válasz",
            "rmdir_failed": "a mappa törlése sikertelen (rmdir_failed)",
            "bad MKDIR reply": "Hibás mappalétrehozási válasz",
            "bad WRITE_BEGIN reply": "Hibás íráskezdési válasz",
            "bad WRITE_DATA reply": "Hibás írási adatválasz",
            "bad WRITE_END reply": "Hibás írászárási válasz",
            "bad READ_END reply": "Hibás olvasási záróválasz",
            "bad REBOOT reply": "Hibás újraindítási válasz",
            "pyserial not installed": "A pyserial nincs telepítve",
            "write failed after retries for": "Az írás többszöri próbálkozás után is sikertelen ennél:",
            "write failed after safe fallback for": "Az írás a gyors mód és a biztonsági visszaesés után is sikertelen ennél:",
            "ERR|WRITE_BEGIN|open_failed": "Hibás íráskezdési válasz: a fájl írásra nem nyitható meg (open_failed)",
            "open_failed": "a fájl írásra nem nyitható meg (open_failed)",
        }
        out = text
        for src, dst in replacements.items():
            out = out.replace(src, dst)
        return out

    def run_job(self, fn, done=None):
        if self.worker and self.worker.is_alive():
            messagebox.showwarning(self.tr("warning"), self.tr("queue_running"))
            return False

        def wrap():
            try:
                result = fn()
                if done:
                    self.after(0, lambda: done(result))
            except Exception as e:
                last_step = self.status_var.get()
                self.after(0, lambda: messagebox.showerror(self.tr("error"), f"{self._localize_error(str(e))}\n\n{self.tr('last_step')}: {last_step}"))
                self.set_status(self.tr("error"))
                self.after(0, self._reset_queue_runtime_labels)
            finally:
                pass

        self.worker = threading.Thread(target=wrap, daemon=True)
        self.worker.start()
        return True

    def refresh_ports(self):
        if list_ports is None:
            return
        ports = []
        for p in list_ports.comports():
            ports.append(f"{p.device} - {p.description}")
        self.port_combo["values"] = ports or [self.tr("ports_none")]
        self.port_var.set(ports[0] if ports else self.tr("ports_none"))

    def _selected_port(self) -> str:
        value = self.port_var.get().strip()
        if not value or value == self.tr("ports_none"):
            raise ProtoError(self.tr("ports_none"))
        return value.split(" - ", 1)[0].strip()

    def ensure_connected(self):
        if not self.client.ser:
            raise ProtoError(self.tr("connect_first"))

    def connect(self):
        def job():
            self.set_status(self.tr("status_connecting"))
            self.client.connect(self._selected_port())
            self.set_status(self.tr("status_maintenance"))
            self.client.begin_maintenance()
            self.client.ping()
            self.set_status(self.tr("status_listing"))
            return self.client.list_files()

        def done(files):
            files = self._apply_known_remote_paths(files)
            self.files = files
            self._rebuild_known_remote_dirs(files)
            self.populate_tree()
            self.set_status(self.tr("maintenance_ok"))

        self.run_job(job, done)

    def disconnect(self):
        if self.queue_running:
            self.cancel_queue()
        self.client.disconnect()
        self.set_status(self.tr("status_ready"))
        self._reset_queue_runtime_labels()

    def enter_maintenance(self):
        def job():
            self.set_status(self.tr("status_maintenance"))
            self.client.begin_maintenance()
            self.client.ping()
            return True

        def done(_):
            self.set_status(self.tr("maintenance_ok"))

        self.run_job(job, done)

    def refresh_list(self):
        def job():
            self.ensure_connected()
            self.set_status(self.tr("status_listing"))
            return self.client.list_files()

        def done(files):
            files = self._apply_known_remote_paths(files)
            self.files = files
            self._rebuild_known_remote_dirs(files)
            self.populate_tree()
            self.set_status(f"{len(files)} {self.tr('file')}")

        self.run_job(job, done)

    def _remember_remote_path(self, path: str, size: int = 0, is_dir: bool = False):
        path = normalize_remote_path(path)
        if path == "/":
            self.known_remote_dir_paths.add("/")
            return
        if is_dir:
            self.known_remote_dir_paths.add(path)
        else:
            name = Path(path).name.lower()
            self.known_remote_file_paths[(name, int(size))] = path
            self.known_remote_file_paths[(name, -1)] = path
        current = ""
        parts = [part for part in path.strip("/").split("/") if part]
        dir_parts = parts if is_dir else parts[:-1]
        for part in dir_parts:
            current += "/" + part
            self.known_remote_dir_paths.add(current)

    def _guess_folder_for_basename_only_file(self, name: str) -> str | None:
        """
        Some maintenance firmwares report files inside folders as basename-only
        entries through the LIST protocol, even though the real filesystem path
        is e.g. /fonts/test_24.vlw.  ESPConnect shows those paths correctly,
        but this serial protocol can lose the parent folder.  Keep the GUI useful
        by restoring well-known myRadio asset folders for basename-only entries.
        """
        low = (name or "").lower()
        if low.endswith(".vlw"):
            return "/fonts"
        return None

    def _apply_known_remote_paths(self, files: list[RemoteFile]) -> list[RemoteFile]:
        out: list[RemoteFile] = []
        seen: set[tuple[str, bool]] = set()
        for rf in files:
            path = normalize_remote_path(rf.path)
            is_dir = getattr(rf, "is_dir", False)
            size = int(getattr(rf, "size", 0))
            if is_dir:
                fixed_path = path
            elif path.count("/") <= 1:
                # LIST returned a root-level basename. If we have seen the same file
                # uploaded into a folder, restore that folder path for display/actions.
                # If this is a known myRadio asset type (for example VLW fonts),
                # restore the conventional folder even after restarting the manager.
                name = Path(path).name.lower()
                remembered = self.known_remote_file_paths.get((name, size)) or self.known_remote_file_paths.get((name, -1))
                guessed_folder = self._guess_folder_for_basename_only_file(name)
                fixed_path = remembered or (normalize_remote_path(f"{guessed_folder}/{name}") if guessed_folder else path)
                if fixed_path != path:
                    self._remember_remote_path(fixed_path, size, False)
            else:
                fixed_path = path
                self._remember_remote_path(fixed_path, size, False)
            key = (fixed_path, is_dir)
            if key not in seen:
                out.append(RemoteFile(fixed_path, size, is_dir))
                seen.add(key)
        for d in sorted(self.known_remote_dir_paths, key=lambda x: (x.count("/"), x.lower())):
            if d != "/" and (d, True) not in seen:
                out.append(RemoteFile(d, 0, True))
                seen.add((d, True))
        return sorted(out, key=lambda x: (x.path.lower(), not x.is_dir))

    def _rebuild_known_remote_dirs(self, files: list[RemoteFile]):
        dirs = {"/"}
        for rf in files:
            path = normalize_remote_path(rf.path)
            parts = [p for p in path.strip('/').split('/') if p]
            current = ""
            end = len(parts) if rf.is_dir else max(0, len(parts) - 1)
            for part in parts[:end]:
                current += "/" + part
                dirs.add(current)
        self.known_remote_dirs = dirs

    def _capture_tree_state(self) -> dict:
        state = {
            "open_paths": set(),
            "selected_paths": [],
            "focus_path": None,
            "yview": 0.0,
        }
        try:
            for item_id in self.tree.selection():
                path, _ = self._item_remote_path(item_id)
                state["selected_paths"].append(path)
        except Exception:
            pass
        try:
            focus_id = self.tree.focus()
            if focus_id:
                state["focus_path"], _ = self._item_remote_path(focus_id)
        except Exception:
            pass
        try:
            state["yview"] = float(self.tree.yview()[0])
        except Exception:
            pass

        def walk(parent=""):
            for child in self.tree.get_children(parent):
                try:
                    path, is_file = self._item_remote_path(child)
                    if not is_file and self.tree.item(child, "open"):
                        state["open_paths"].add(path)
                except Exception:
                    pass
                walk(child)

        walk("")
        return state

    def populate_tree(self, restore_state: dict | None = None):
        self.tree.delete(*self.tree.get_children())
        self.tree_item_info = {}

        # Some ESP32 SPIFFS/LittleFS list implementations return only FILE rows,
        # even when the file path contains folders.  Build a real visual folder
        # tree from every slash-separated path, and only mark the final segment as
        # a file when the remote entry itself is not a directory.
        nodes = {"/": ""}
        entry_by_path: dict[str, RemoteFile] = {}
        for rf in self.files:
            path = normalize_remote_path(rf.path)
            if path == "/":
                continue
            entry_by_path[path] = RemoteFile(path, rf.size, getattr(rf, "is_dir", False))

        def ensure_dir(path: str) -> str:
            path = normalize_remote_path(path)
            if path in nodes:
                return nodes[path]
            parent_path = normalize_remote_path("/".join(path.rstrip("/").split("/")[:-1]) or "/")
            parent_id = ensure_dir(parent_path) if parent_path != path else ""
            name = path.rstrip("/").split("/")[-1]
            item_id = self.tree.insert(parent_id, "end", text=name, values=(self.tr("folder"), ""))
            nodes[path] = item_id
            self.tree_item_info[item_id] = (path, False)
            return item_id

        for path in sorted(entry_by_path, key=lambda x: (x.count("/"), x.lower())):
            rf = entry_by_path[path]
            parts = [part for part in path.strip("/").split("/") if part]
            if not parts:
                continue

            parent_path = "/"
            for part in parts[:-1] if not rf.is_dir else parts:
                parent_path = normalize_remote_path(parent_path.rstrip("/") + "/" + part)
                ensure_dir(parent_path)

            if rf.is_dir:
                continue

            parent_path = normalize_remote_path("/".join(path.split("/")[:-1]) or "/")
            parent_id = ensure_dir(parent_path)
            name = parts[-1]
            item_id = self.tree.insert(parent_id, "end", text=name, values=(self.tr("file"), fmt_size(rf.size)))
            nodes[path] = item_id
            self.tree_item_info[item_id] = (path, True)

        if restore_state:
            for path in restore_state.get("open_paths", set()):
                item_id = nodes.get(path)
                if item_id:
                    self.tree.item(item_id, open=True)

            selected_ids = []
            for path in restore_state.get("selected_paths", []):
                item_id = nodes.get(path)
                if item_id:
                    selected_ids.append(item_id)
            if selected_ids:
                self.tree.selection_set(selected_ids)

            focus_path = restore_state.get("focus_path")
            if focus_path and focus_path in nodes:
                self.tree.focus(nodes[focus_path])

            def restore_view():
                try:
                    yview = float(restore_state.get("yview", 0.0))
                    self.tree.yview_moveto(yview)
                    selected = self.tree.selection()
                    if selected:
                        self.tree.see(selected[0])
                except Exception:
                    pass
                self._schedule_tree_scrollbar_refresh()

            self.after_idle(restore_view)
        else:
            self._schedule_tree_scrollbar_refresh()

    def _item_remote_path(self, item_id: str) -> tuple[str, bool]:
        info = self.tree_item_info.get(item_id)
        if info:
            return info
        parts = []
        current = item_id
        while current:
            parts.append(self.tree.item(current, "text"))
            current = self.tree.parent(current)
        parts.reverse()
        path = normalize_remote_path("/" + "/".join(parts))
        is_file = self.tree.set(item_id, "type") == self.tr("file")
        return path, is_file

    def _selected_upload_target_root(self) -> str:
        selection = self.tree.selection()
        if not selection:
            return "/"
        selected_remote, selected_is_file = self._item_remote_path(selection[0])
        if selected_is_file:
            return normalize_remote_path("/".join(selected_remote.split("/")[:-1]) or "/")
        return normalize_remote_path(selected_remote)

    def _queue_append(self, task: UploadTask):
        with self.queue_lock:
            self.upload_queue.append(task)
        self.after(0, self.refresh_queue_tree)

    def queue_add_files(self):
        paths = filedialog.askopenfilenames()
        if not paths:
            return
        target_root = self._selected_upload_target_root()
        for file_path in paths:
            p = Path(file_path)
            task = UploadTask(local_path=p, remote_path=normalize_remote_path(f"{target_root}/{p.name}"), size=p.stat().st_size)
            self._queue_append(task)
        self.set_status(self.tr("queue_added"))

    def queue_add_folder(self):
        folder = filedialog.askdirectory()
        if not folder:
            return
        root = Path(folder)
        files = [p for p in root.rglob("*") if p.is_file()]
        if not files:
            messagebox.showwarning(self.tr("warning"), self.tr("empty_folder"))
            return
        selection = self.tree.selection()
        selected_remote = None
        selected_is_file = False
        if selection:
            selected_remote, selected_is_file = self._item_remote_path(selection[0])
        if selected_remote:
            if selected_is_file:
                target_root = normalize_remote_path("/".join(selected_remote.split("/")[:-1]) or "/")
                preserve_local_folder_name = False
            else:
                target_root = normalize_remote_path(selected_remote)
                preserve_local_folder_name = True
        else:
            target_root = "/"
            preserve_local_folder_name = True
        root_name = root.name.strip("/\\")
        remote_base = normalize_remote_path(f"{target_root}/{root_name}" if root_name else target_root)
        for p in files:
            rel = p.relative_to(root).as_posix()
            remote_path = normalize_remote_path(f"{remote_base}/{rel}")
            task = UploadTask(local_path=p, remote_path=remote_path, size=p.stat().st_size)
            self._queue_append(task)
        self.set_status(self.tr("queue_added"))

    def refresh_queue_tree(self):
        selected = set(self.queue_tree.selection())
        self.queue_tree.delete(*self.queue_tree.get_children())
        failures = 0
        with self.queue_lock:
            tasks = list(self.upload_queue)
        for task in tasks:
            if task.status == "failed":
                failures += 1
            item = self.queue_tree.insert(
                "",
                "end",
                iid=task.task_id,
                text=task.local_path.name,
                values=(task.remote_path, self._task_status_label(task.status), f"{task.progress_pct:.0f}%", fmt_size(task.size)),
            )
            if item in selected:
                self.queue_tree.selection_add(item)
        self.failures_var.set(str(failures))
        self.after_idle(self._resize_queue_tree_columns)
        self._schedule_queue_tree_scrollbar_refresh()
        if not tasks:
            self.current_file_var.set("-")
            self.overall_var.set("0 / 0")
            if not self.queue_running:
                self.set_status(self.tr("queue_idle"))

    def _task_status_label(self, status: str) -> str:
        mapping = {
            "waiting": self.tr("queue_waiting"),
            "uploading": self.tr("queue_uploading"),
            "done": self.tr("queue_done"),
            "failed": self.tr("queue_failed"),
            "cancelled": self.tr("queue_cancelled"),
            "retrying": self.tr("queue_retrying"),
        }
        return mapping.get(status, status)

    def remove_selected_tasks(self):
        if self.queue_running:
            messagebox.showwarning(self.tr("warning"), self.tr("queue_running"))
            return
        selected = set(self.queue_tree.selection())
        if not selected:
            return
        with self.queue_lock:
            self.upload_queue = [t for t in self.upload_queue if t.task_id not in selected]
        self.refresh_both_views()

    def clear_completed_tasks(self):
        if self.queue_running:
            messagebox.showwarning(self.tr("warning"), self.tr("queue_running"))
            return
        with self.queue_lock:
            self.upload_queue = [t for t in self.upload_queue if t.status not in {"done", "cancelled"}]
        self.refresh_queue_tree()

    def retry_failed_tasks(self):
        if self.queue_running:
            messagebox.showwarning(self.tr("warning"), self.tr("queue_running"))
            return
        changed = False
        with self.queue_lock:
            for task in self.upload_queue:
                if task.status == "failed":
                    task.status = "waiting"
                    task.progress_pct = 0.0
                    task.uploaded_bytes = 0
                    task.error = ""
                    changed = True
        if changed:
            self.refresh_queue_tree()
            self.set_status(self.tr("queue_added"))

    def cancel_queue(self):
        if not self.queue_running:
            return
        self.cancel_event.set()
        self.set_status(self.tr("queue_cancel_requested"))


    def _current_used_bytes(self) -> int:
        return sum(rf.size for rf in self.files)

    def _estimated_free_bytes(self) -> int | None:
        if self.spiffs_capacity_kb is None:
            return None
        total = self.spiffs_capacity_kb * 1024
        used = self._current_used_bytes()
        return max(0, total - used)

    def _pending_queue_bytes(self) -> int:
        with self.queue_lock:
            return sum(t.size for t in self.upload_queue if t.status in {"waiting", "retrying"})

    def _preflight_check_available_space(self) -> bool:
        free_bytes = self._estimated_free_bytes()
        pending_bytes = self._pending_queue_bytes()
        if free_bytes is None or pending_bytes <= 0:
            return True
        if pending_bytes > free_bytes:
            messagebox.showwarning(
                self.tr("warning"),
                self.tr("space_check_insufficient").format(
                    free=fmt_size(free_bytes),
                    need=fmt_size(pending_bytes),
                ),
            )
            return False
        safety_floor = 96 * 1024
        if free_bytes - pending_bytes < safety_floor:
            return messagebox.askyesno(
                self.tr("warning"),
                self.tr("space_check_low").format(
                    free=fmt_size(free_bytes),
                    need=fmt_size(pending_bytes),
                ),
            )
        return True

    def _is_critical_spiffs_write_error(self, error_text: str) -> bool:
        return is_probable_spiffs_open_failed(error_text)

    def start_queue(self):
        if self.queue_running:
            messagebox.showwarning(self.tr("warning"), self.tr("queue_running"))
            return
        with self.queue_lock:
            pending = [t for t in self.upload_queue if t.status in {"waiting", "retrying"}]
        if not pending:
            messagebox.showwarning(self.tr("warning"), self.tr("queue_empty_start"))
            return
        if not self._preflight_check_available_space():
            return

        self.queue_stop_reason = None
        self.queue_running = True
        self.cancel_event.clear()
        self._set_queue_controls_enabled(False)
        self.set_status(self.tr("status_uploading"))

        def job():
            try:
                self.ensure_connected()
                self._run_upload_queue()
                return True
            finally:
                self.queue_running = False

        def done(_):
            self._set_queue_controls_enabled(True)
            self.clear_completed_tasks()
            self.refresh_queue_tree()
            if self.queue_stop_reason:
                self.set_status(self.queue_stop_reason)
                messagebox.showwarning(self.tr("warning"), self.queue_stop_reason)
            elif self.cancel_event.is_set():
                self.set_status(self.tr("queue_cancelled_done"))
            else:
                self.set_status(self.tr("queue_finished"))
            self.cancel_event.clear()
            self.refresh_both_views(background=True)
            self._reset_queue_runtime_labels(keep_status=True)

        started = self.run_job(job, done)
        if not started:
            self.queue_running = False
            self._set_queue_controls_enabled(True)

    def _set_queue_controls_enabled(self, enabled: bool):
        state = tk.NORMAL if enabled else tk.DISABLED
        self.after(0, lambda: self.btn_queue_add_files.config(state=state))
        self.after(0, lambda: self.btn_queue_add_folder.config(state=state))
        self.after(0, lambda: self.btn_queue_start.config(state=state))
        self.after(0, lambda: self.btn_queue_cancel.config(state=tk.NORMAL if not enabled else tk.DISABLED))
        self.after(0, lambda: self.btn_queue_remove.config(state=state))

    def _reset_queue_runtime_labels(self, keep_status: bool = False):
        self.queue_stop_reason = None
        self.progress_var.set(0.0)
        self.overall_progress_var.set(0.0)
        self.current_file_var.set("-")
        self.overall_var.set("0 / 0")
        self.speed_var.set("0 KB/s")
        self.eta_var.set("--:--")
        if not keep_status:
            self.set_status(self.tr("status_ready"))

    def _ensure_remote_parent_dirs(self, remote_path: str):
        parent = "/".join(normalize_remote_path(remote_path).split("/")[:-1]) or "/"
        if parent == "/":
            return
        current_path = ""
        for part in [p for p in parent.strip("/").split("/") if p]:
            current_path += "/" + part
            if current_path in self.known_remote_dirs:
                continue
            try:
                self.client.mkdir(current_path)
            except Exception as e:
                msg = str(e).lower()
                if not any(token in msg for token in ("exists", "exist", "already", "mkdir", "bad mkdir reply")):
                    raise
            self.known_remote_dirs.add(current_path)

    def _run_upload_queue(self):
        with self.queue_lock:
            tasks = [t for t in self.upload_queue if t.status in {"waiting", "retrying"}]
            total_bytes = sum(t.size for t in tasks)
        sent_before = 0
        queue_start = time.time()
        total_count = len(tasks)
        current_index = 0
        for task in tasks:
            current_index += 1
            if self.cancel_event.is_set():
                self._mark_waiting_as_cancelled()
                break
            self.current_queue_task_id = task.task_id
            self.after(0, lambda n=task.local_path.name: self.current_file_var.set(n))
            self.after(0, lambda i=current_index, tc=total_count: self.overall_var.set(f"{i} / {tc}"))
            ok = self._upload_single_task(task, current_index, total_count, sent_before, total_bytes, queue_start)
            sent_before += task.size if ok else task.uploaded_bytes
        self.current_queue_task_id = None

    def _mark_waiting_as_cancelled(self):
        with self.queue_lock:
            for task in self.upload_queue:
                if task.status in {"waiting", "retrying"}:
                    task.status = "cancelled"
        self.after(0, self.refresh_queue_tree)

    def _set_transfer_metrics(self, current_name: str, current_index: int, total_count: int, bytes_done: int, bytes_total: int, start_time: float):
        elapsed = max(0.25, time.time() - start_time)
        speed = bytes_done / elapsed if bytes_done > 0 else 0.0
        eta = (max(0, bytes_total - bytes_done) / speed) if speed > 0 and bytes_done >= 512 else None
        pct = 100.0 if bytes_total == 0 else (bytes_done / bytes_total) * 100.0
        self.after(0, lambda n=current_name: self.current_file_var.set(n))
        self.after(0, lambda i=current_index, tc=total_count: self.overall_var.set(f"{i} / {tc}"))
        self.after(0, lambda p=pct: self.progress_var.set(p))
        self.after(0, lambda p=pct: self.overall_progress_var.set(p))
        self.after(0, lambda s=human_speed(speed): self.speed_var.set(s))
        self.after(0, lambda e=format_eta(eta): self.eta_var.set(e))

    def _reset_transfer_metrics(self):
        self.progress_var.set(0.0)
        self.overall_progress_var.set(0.0)
        self.current_file_var.set("-")
        self.overall_var.set("0 / 0")
        self.speed_var.set("0 KB/s")
        self.eta_var.set("--:--")

    def _upload_single_task(self, task: UploadTask, current_index: int, total_count: int, sent_before: int, total_bytes: int, queue_start: float) -> bool:
        local_bytes = task.local_path.read_bytes()
        last_error = None
        for attempt in range(task.retries_done, task.max_retries + 1):
            if self.cancel_event.is_set():
                task.status = "cancelled"
                self.after(0, self.refresh_queue_tree)
                return False
            task.status = "uploading" if attempt == 0 else "retrying"
            task.error = ""
            task.progress_pct = 0.0
            task.uploaded_bytes = 0
            self.after(0, self.refresh_queue_tree)
            try:
                self._ensure_remote_parent_dirs(task.remote_path)
                file_start = time.time()
                for chunk_idx, chunk_total, uploaded in self.client.write_file(task.remote_path, local_bytes):
                    if self.cancel_event.is_set():
                        try:
                            self.client.abort_write()
                        except Exception:
                            pass
                        task.status = "cancelled"
                        self.after(0, self.refresh_queue_tree)
                        return False
                    task.uploaded_bytes = uploaded
                    task.progress_pct = 100.0 if task.size == 0 else (uploaded / max(1, task.size)) * 100.0
                    elapsed = max(0.001, time.time() - queue_start)
                    total_sent_now = sent_before + uploaded
                    speed = total_sent_now / elapsed
                    remaining = max(0, total_bytes - total_sent_now)
                    eta = remaining / speed if speed > 0 else None
                    self.after(0, lambda p=task.progress_pct: self.progress_var.set(p))
                    overall_pct = 100.0 if total_bytes == 0 else (total_sent_now / total_bytes) * 100.0
                    self.after(0, lambda p=overall_pct: self.overall_progress_var.set(p))
                    self.after(0, lambda s=human_speed(speed): self.speed_var.set(s))
                    self.after(0, lambda e=format_eta(eta): self.eta_var.set(e))
                    self.set_status(f"{self.tr('status_uploading')} {current_index}/{total_count} - {task.remote_path}")
                    self.after(0, self.refresh_queue_tree)
                task.status = "done"
                task.progress_pct = 100.0
                task.uploaded_bytes = task.size
                self._remember_remote_path(task.remote_path, task.size, False)
                self.after(0, self.refresh_queue_tree)
                return True
            except Exception as e:
                task.retries_done = attempt + 1
                raw_error = str(e)
                if self._is_critical_spiffs_write_error(raw_error):
                    localized = build_open_failed_hint(self._localize_error(raw_error), self.tr("open_failed_hint"))
                    task.error = localized
                    task.status = "failed"
                    self.queue_stop_reason = self.tr("critical_spiffs_write_error") + "\n\n" + localized
                    self.cancel_event.set()
                    self.after(0, self.refresh_queue_tree)
                    return False
                task.error = self._localize_error(raw_error)
                last_error = e
                if attempt < task.max_retries and not self.cancel_event.is_set():
                    task.status = "retrying"
                    self.set_status(f"{self.tr('queue_retrying')}: {task.remote_path} | {self._localize_error(raw_error)}")
                    self.after(0, self.refresh_queue_tree)
                    time.sleep(0.5 * (attempt + 1))
                    continue
                task.status = "failed"
                self.after(0, self.refresh_queue_tree)
                return False
        if last_error:
            task.error = str(last_error)
        task.status = "failed"
        self.after(0, self.refresh_queue_tree)
        return False

    def refresh_both_views(self, background: bool = False):
        def job():
            if self.client.ser:
                files = self.client.list_files()
            else:
                files = self.files
            return files

        tree_state = self._capture_tree_state()

        def done(files):
            files = self._apply_known_remote_paths(files)
            self.files = files
            self._rebuild_known_remote_dirs(files)
            self.populate_tree(restore_state=tree_state)
            self.refresh_queue_tree()

        if background:
            self.run_job(job, done)
        else:
            done(self.client.list_files() if self.client.ser else self.files)

    def backup_zip(self):
        out = filedialog.asksaveasfilename(title=self.tr("save_backup_title"), defaultextension=".zip", filetypes=[("ZIP", "*.zip")], initialfile="myradio_spiffs_mentes.zip" if self.lang == "HU" else "myradio_spiffs_backup.zip")
        if not out:
            return
        out_path = Path(out)

        def job():
            self.ensure_connected()
            files = self.client.list_files()
            if not files:
                raise ProtoError(self.tr("no_files"))
            self.set_status(self.tr("status_saving"))
            total_files = len(files)
            total_bytes = sum(rf.size for rf in files)
            transferred = 0
            start = time.time()
            self.after(0, self._reset_transfer_metrics)
            with zipfile.ZipFile(out_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
                for idx, rf in enumerate(files, 1):
                    data = self.client.read_file(rf.path)
                    zf.writestr(rf.path.lstrip("/"), data)
                    transferred += len(data)
                    self._set_transfer_metrics(Path(rf.path).name, idx, total_files, transferred, total_bytes, start)
                    self.set_status(f"{self.tr('status_saving')} {idx}/{total_files} - {rf.path}")
            return True

        def done(_):
            self._reset_transfer_metrics()
            self.refresh_both_views(background=True)
            self.set_status(self.tr("backup_done"))
            messagebox.showinfo(self.tr("done"), self.tr("backup_done"))

        self.run_job(job, done)

    def restore_zip(self):
        zpath = filedialog.askopenfilename(title=self.tr("open_backup_title"), filetypes=[("ZIP", "*.zip")])
        if not zpath:
            return
        zp = Path(zpath)

        def job():
            self.ensure_connected()
            self.set_status(self.tr("status_restoring"))
            current = self.client.list_files()
            for rf in sorted(current, key=lambda x: (x.path.count("/"), x.path.lower()), reverse=True):
                self.client.delete_file(rf.path)
            with zipfile.ZipFile(zp, "r") as zf:
                names = [n for n in zf.namelist() if not n.endswith("/")]
                total_files = max(1, len(names))
                total_bytes = sum(len(zf.read(name)) for name in names) if names else 0
                transferred = 0
                start = time.time()
                self.after(0, self._reset_transfer_metrics)
                for idx, name in enumerate(names, 1):
                    data = zf.read(name)
                    remote_path = normalize_remote_path("/" + name)
                    self._ensure_remote_parent_dirs(remote_path)
                    for _, _, uploaded in self.client.write_file(remote_path, data):
                        self._set_transfer_metrics(Path(name).name, idx, total_files, transferred + uploaded, total_bytes, start)
                        self.set_status(f"{self.tr('status_restoring')} {idx}/{total_files} - {remote_path}")
                    transferred += len(data)
            return True

        def done(_):
            self._reset_transfer_metrics()
            self.refresh_both_views(background=True)
            messagebox.showinfo(self.tr("done"), self.tr("restore_done"))

        self.run_job(job, done)

    def create_directory(self):
        base = self._selected_upload_target_root()
        name = simpledialog.askstring(
            self.tr("mkdir"),
            self.tr("enter_dir_name"),
            parent=self,
        )
        if name is None:
            return
        name = name.strip().strip("/\\")
        if not name:
            return
        remote_path = normalize_remote_path(base.rstrip("/") + "/" + name if base != "/" else "/" + name)

        def job():
            self.ensure_connected()
            self.set_status(self.tr("status_mkdir"))
            self.client.mkdir(remote_path)
            return True

        def done(_):
            self._remember_remote_path(remote_path, 0, True)
            self.refresh_both_views(background=True)
            messagebox.showinfo(self.tr("done"), self.tr("mkdir_done"))

        self.run_job(job, done)

    def download_selected(self):
        selection = self.tree.selection()
        if not selection:
            messagebox.showwarning(self.tr("warning"), self.tr("tree_no_selection"))
            return
        path, is_file = self._item_remote_path(selection[0])
        if not is_file:
            messagebox.showwarning(self.tr("warning"), self.tr("tree_no_selection"))
            return
        out = filedialog.asksaveasfilename(title=self.tr("save_selected_title"), initialfile=Path(path).name)
        if not out:
            return
        out_path = Path(out)

        def job():
            self.ensure_connected()
            self.set_status(self.tr("status_downloading"))
            data = self.client.read_file(path)
            out_path.write_bytes(data)
            return True

        def done(_):
            self.refresh_both_views(background=True)
            messagebox.showinfo(self.tr("done"), self.tr("download_done"))

        self.run_job(job, done)

    def delete_selected(self):
        selection = self.tree.selection()
        if not selection:
            messagebox.showwarning(self.tr("warning"), self.tr("tree_no_selection"))
            return

        selected_items = [self._item_remote_path(item_id) for item_id in selection]

        def job():
            self.ensure_connected()
            files = self.client.list_files()
            all_file_paths = [f.path for f in files if not getattr(f, "is_dir", False)]
            all_dir_paths = [f.path for f in files if getattr(f, "is_dir", False)]
            target_files = set()
            target_dirs = set()
            requested_paths = []
            for path, is_file in selected_items:
                path = normalize_remote_path(path)
                requested_paths.append((path, is_file))
                if is_file:
                    target_files.add(path)
                else:
                    prefix = path.rstrip("/") + "/"
                    for file_path in all_file_paths:
                        if file_path == path or file_path.startswith(prefix):
                            target_files.add(file_path)
                    for dir_path in all_dir_paths:
                        if dir_path == path or dir_path.startswith(prefix):
                            target_dirs.add(dir_path)
                    target_dirs.add(path)

            for target in sorted(target_files, key=lambda x: (x.count("/"), x.lower()), reverse=True):
                self.client.delete_file(target)

            dir_errors = []
            for target in sorted(target_dirs, key=lambda x: (x.count("/"), x.lower()), reverse=True):
                if target == "/":
                    continue
                try:
                    self.client.delete_file(target)
                except Exception as delete_error:
                    try:
                        self.client.rmdir(target)
                    except Exception as rmdir_error:
                        dir_errors.append(f"{target}: {rmdir_error or delete_error}")

            remaining = self.client.list_files()
            remaining_paths = [f.path for f in remaining]
            still_present = []
            for path, is_file in requested_paths:
                if is_file:
                    if path in remaining_paths:
                        still_present.append(path)
                else:
                    prefix = path.rstrip("/") + "/"
                    for remote_path in remaining_paths:
                        if remote_path == path or remote_path.startswith(prefix):
                            still_present.append(remote_path)
                            break
            if still_present:
                detail = still_present[0]
                if dir_errors:
                    detail += " | " + dir_errors[0]
                raise ProtoError(f"delete verification failed: {detail}")
            return True

        def done(_):
            self.refresh_both_views(background=True)
            messagebox.showinfo(self.tr("done"), self.tr("delete_done"))

        self.run_job(job, done)

    def reboot_radio(self):
        def job():
            self.ensure_connected()
            self.set_status(self.tr("status_rebooting"))
            self.client.reboot()
            return True

        def done(_):
            self.disconnect()
            messagebox.showinfo(self.tr("done"), self.tr("reboot_done"))

        self.run_job(job, done)


def main():
    app = App()
    app.mainloop()


if __name__ == "__main__":
    main()
