"""Local UDP -> BLE NuS RPM bridge for ETS2 ESP32 panel.

Listens only on 127.0.0.1:29100. A local telemetry source must send either
an integer RPM value (e.g. b"1420") or b"RPM:1420" as a UDP datagram.
The script then writes that RPM line over the ESP32's BLE Nordic UART Service.
"""
import asyncio
import socket
import threading
import time
from bleak import BleakClient, BleakScanner

DEVICE_NAME_HINTS = ("ETS2 Custom Wheel", "ETS2-NuS")
NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
UDP_HOST = "127.0.0.1"
UDP_PORT = 29100

latest_rpm = None
latest_lock = threading.Lock()


def udp_listener():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_HOST, UDP_PORT))
    print(f"UDP RPM listener: {UDP_HOST}:{UDP_PORT}")
    while True:
        data, _addr = sock.recvfrom(256)
        text = data.decode("ascii", errors="ignore").strip()
        if text.lower().startswith("rpm:"):
            text = text.split(":", 1)[1].strip()
        try:
            rpm = int(float(text))
        except ValueError:
            print(f"Ignored non-RPM datagram: {text[:60]!r}")
            continue
        if not 0 <= rpm <= 30000:
            continue
        with latest_lock:
            global latest_rpm
            latest_rpm = rpm


async def find_device():
    print("Searching BLE for the panel (active scan)...")
    devices = await BleakScanner.discover(timeout=8.0, return_adv=True)
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
    threading.Thread(target=udp_listener, daemon=True).start()
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
                # Discover the NUS RX characteristic after connection.
                chars = {c.uuid.lower() for s in client.services for c in s.characteristics}
                if NUS_RX_UUID not in chars:
                    print("NUS RX characteristic not visible. Remove/re-pair the device in Windows and retry.")
                    await asyncio.sleep(3)
                    continue
                while client.is_connected:
                    with latest_lock:
                        rpm = latest_rpm
                    now = time.monotonic()
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
