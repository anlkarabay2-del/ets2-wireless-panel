"""Local UDP -> BLE NuS RPM bridge for ETS2 ESP32 panel.

Listens only on 127.0.0.1:29100. A local telemetry source sends an integer RPM
value (e.g. b"1420") or b"RPM:1420" as a UDP datagram. The script forwards it
over the ESP32's BLE Nordic UART Service.
"""
import asyncio
import socket
import threading
import time
from typing import Optional

DEVICE_NAME_HINTS = ("ETS2 Custom Wheel", "ETS2-NuS")
NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
UDP_HOST = "127.0.0.1"
UDP_PORT = 29100
UDP_STALE_SECONDS = 1.0

latest_rpm: Optional[int] = None
latest_rpm_received_at: Optional[float] = None
latest_lock = threading.Lock()
udp_ready = threading.Event()
udp_start_error: Optional[OSError] = None


def parse_rpm_datagram(data: bytes) -> Optional[int]:
    """Parse a local UDP payload into RPM, returning None if it is invalid."""
    text = data.decode("ascii", errors="ignore").strip()
    if text.lower().startswith("rpm:"):
        text = text.split(":", 1)[1].strip()

    try:
        rpm = int(float(text))
    except (ValueError, OverflowError):
        return None

    if not 0 <= rpm <= 30000:
        return None
    return rpm


def effective_rpm(rpm: Optional[int], received_at: Optional[float], now: float) -> Optional[int]:
    """Return None before the first sample and zero when telemetry goes stale."""
    if rpm is None:
        return None
    if received_at is None or now - received_at > UDP_STALE_SECONDS:
        return 0
    return rpm


def udp_listener():
    global latest_rpm, latest_rpm_received_at, udp_start_error

    sock = None
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind((UDP_HOST, UDP_PORT))
    except OSError as exc:
        udp_start_error = exc
        udp_ready.set()
        if sock is not None:
            sock.close()
        return

    udp_ready.set()
    with sock:
        print(f"UDP RPM listener: {UDP_HOST}:{UDP_PORT}")
        while True:
            data, _addr = sock.recvfrom(256)
            rpm = parse_rpm_datagram(data)
            if rpm is None:
                text = data.decode("ascii", errors="ignore").strip()
                print(f"Ignored invalid RPM datagram: {text[:60]!r}")
                continue

            with latest_lock:
                latest_rpm = rpm
                latest_rpm_received_at = time.monotonic()


async def find_device():
    from bleak import BleakScanner

    print("Searching BLE for the panel (active scan)...")
    try:
        devices = await BleakScanner.discover(timeout=8.0, return_adv=True)
    except Exception as exc:
        print(f"BLE scan failed: {exc}")
        return None

    for _address, pair in devices.items():
        device, adv = pair
        uuids = [u.lower() for u in (adv.service_uuids or [])]
        name = device.name or adv.local_name or ""
        if NUS_SERVICE_UUID in uuids or any(hint.lower() in name.lower() for hint in DEVICE_NAME_HINTS):
            print(f"Found: {name or '(unnamed)'} [{device.address}]")
            return device
    print("Panel not found. Check that firmware is running and Windows Bluetooth is enabled.")
    print("If it was paired before a firmware change, remove it in Windows Bluetooth settings and pair again.")
    return None


async def main():
    from bleak import BleakClient

    threading.Thread(target=udp_listener, daemon=True).start()
    while not udp_ready.is_set():
        await asyncio.sleep(0.05)
    if udp_start_error is not None:
        raise RuntimeError(
            f"Could not bind UDP listener to {UDP_HOST}:{UDP_PORT}: {udp_start_error}. "
            "Close the other program using this port, then restart the bridge."
        ) from udp_start_error

    last_sent = None
    last_send_time = 0.0
    while True:
        device = await find_device()
        if device is None:
            await asyncio.sleep(3)
            continue
        try:
            async with BleakClient(device, timeout=20.0) as client:
                print("BLE connected. Waiting for local RPM datagrams...")
                chars = {c.uuid.lower() for s in client.services for c in s.characteristics}
                if NUS_RX_UUID not in chars:
                    print("NUS RX characteristic not visible. Remove/re-pair the device in Windows and retry.")
                    await asyncio.sleep(3)
                    continue
                while client.is_connected:
                    with latest_lock:
                        rpm = latest_rpm
                        received_at = latest_rpm_received_at
                    now = time.monotonic()
                    rpm = effective_rpm(rpm, received_at, now)
                    if rpm is not None and (rpm != last_sent or now - last_send_time >= 0.20):
                        await client.write_gatt_char(NUS_RX_UUID, f"{rpm}\n".encode("ascii"), response=False)
                        last_sent = rpm
                        last_send_time = now
                    await asyncio.sleep(0.02)
        except Exception as exc:
            print(f"BLE link ended: {exc}")
            await asyncio.sleep(2)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("Stopped.")
