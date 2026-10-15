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


LED_SCALE_MAX_RPM = 3500
MAX_TELEMETRY_RPM = 30000


def parse_rpm_datagram(data: bytes) -> Optional[int]:
    """Parse RPM telemetry into the firmware's 0-3500 LED scale.

    New SimHub packets use "RPM:<current>:<max>". That format is normalized
    against each vehicle's max RPM so the full LED bar is reachable at redline.
    Legacy integer packets and "RPM:<current>" packets keep their old absolute
    RPM behavior for backwards compatibility.
    """
    try:
        text = data.decode("ascii").strip()
    except UnicodeDecodeError:
        return None

    if text.lower().startswith("rpm:"):
        text = text.split(":", 1)[1].strip()

    parts = text.split(":")
    try:
        if len(parts) == 1:
            # Legacy format: preserve the prior absolute-RPM behavior.
            rpm = int(float(parts[0]))
        elif len(parts) == 2:
            current_rpm = int(float(parts[0]))
            max_rpm = int(float(parts[1]))
            if not 0 <= current_rpm <= MAX_TELEMETRY_RPM:
                return None
            if not 1 <= max_rpm <= MAX_TELEMETRY_RPM:
                return None
            # Integer rounding avoids float drift in the 0-3500 bar scale.
            rpm = (current_rpm * LED_SCALE_MAX_RPM + max_rpm // 2) // max_rpm
            rpm = min(rpm, LED_SCALE_MAX_RPM)
        else:
            return None
    except (ValueError, OverflowError):
        return None

    if not 0 <= rpm <= MAX_TELEMETRY_RPM:
        return None
    return rpm


def effective_rpm(rpm: Optional[int], received_at: Optional[float], now: float) -> Optional[int]:
    """Return None before the first sample and zero when telemetry goes stale."""
    if rpm is None:
        return None
    if received_at is None or now - received_at > UDP_STALE_SECONDS:
        return 0
    return rpm


def is_panel_advertisement(device_name, local_name, service_uuids) -> bool:
    """Match either observed BLE name or the NuS service UUID.

    Windows may return a cached device.name that differs from the local name
    in the current scan response. Check both rather than letting the cached
    name hide a valid adv.local_name match.
    """
    advertised_uuids = {str(uuid).lower() for uuid in (service_uuids or ())}
    if NUS_SERVICE_UUID in advertised_uuids:
        return True

    names = (device_name or "", local_name or "")
    return any(
        hint.lower() in name.lower()
        for hint in DEVICE_NAME_HINTS
        for name in names
        if name
    )


def write_response_mode(characteristic_properties) -> Optional[bool]:
    """Choose a GATT write mode supported by a BLE characteristic.

    NuS-NimBLE-Serial 4.2.1 exposes its RX characteristic as 'write' (with
    response), not 'write-without-response'. Prefer response writes when both
    modes are available, and return None if neither mode is supported.
    """
    properties = {str(value).lower() for value in (characteristic_properties or ())}
    if "write" in properties:
        return True
    if "write-without-response" in properties:
        return False
    return None


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
        if is_panel_advertisement(device.name, adv.local_name, adv.service_uuids):
            name = adv.local_name or device.name or ""
            print(f"Found: {name or '(unnamed)'} [{device.address}]")
            return device
    print("Panel not found. Check that firmware is running and Windows Bluetooth is enabled.")
    print("If it was paired before a firmware change, remove it in Windows Bluetooth settings and retry.")
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
                rx_characteristic = next(
                    (
                        characteristic
                        for service in client.services
                        for characteristic in service.characteristics
                        if characteristic.uuid.lower() == NUS_RX_UUID
                    ),
                    None,
                )
                if rx_characteristic is None:
                    print("NUS RX characteristic not visible. Remove/re-pair the device in Windows and retry.")
                    await asyncio.sleep(3)
                    continue

                response_mode = write_response_mode(rx_characteristic.properties)
                if response_mode is None:
                    print("NUS RX characteristic supports neither write mode; retrying BLE discovery.")
                    await asyncio.sleep(3)
                    continue

                print(
                    "NuS RX write mode: "
                    + ("with response" if response_mode else "without response")
                )
                while client.is_connected:
                    with latest_lock:
                        rpm = latest_rpm
                        received_at = latest_rpm_received_at
                    now = time.monotonic()
                    rpm = effective_rpm(rpm, received_at, now)
                    if rpm is not None and (rpm != last_sent or now - last_send_time >= 0.20):
                        await client.write_gatt_char(
                            rx_characteristic,
                            f"{rpm}\n".encode("ascii"),
                            response=response_mode,
                        )
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
