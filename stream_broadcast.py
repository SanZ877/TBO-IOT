import socket
import sounddevice as sd
import numpy as np

# 1. Gunakan Broadcast IP agar Python bisa dinyalakan kapan saja tanpa restart ESP32
BROADCAST_IP = "10.22.57.195"
PORT = 8889

# Stereo Mix (Index 13)
DEVICE_INDEX = 13

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

dev_info = sd.query_devices(DEVICE_INDEX)
native_sr = int(dev_info['default_samplerate'])
print(f"-> Terhubung ke: {dev_info['name']}")

GAIN = 5.0

def audio_callback(indata, frames, time, status):
    # Indikator visual volume audio laptop
    vol = int(np.linalg.norm(indata[:, 0]) * 20)
    print(f"\rLevel Audio System: [{'#' * min(vol, 30):<30}]", end='', flush=True)

    # Downsample sinyal ke ~16kHz
    step = max(1, int(native_sr / 16000))
    subsampled = indata[::step, 0] * GAIN
    
    # Format Square Wave (0 / 255) untuk buzzer pasif
    audio_8bit = np.where(subsampled > 0.05, 255, 0).astype(np.uint8)
    sock.sendto(audio_8bit.tobytes(), (BROADCAST_IP, PORT))

print("\nStreaming audio laptop ke buzzer ESP32 (UDP Broadcast)...")
print("Putar musik/video di laptop. Tekan Ctrl+C untuk berhenti.\n")

# 2. Tambahkan blocksize=512 agar ukuran paket konstan dan buffer ESP32 tidak meluap
with sd.InputStream(device=DEVICE_INDEX, samplerate=native_sr, channels=2, callback=audio_callback, blocksize=512):
    try:
        while True:
            sd.sleep(1000)
    except KeyboardInterrupt:
        print("\nStreaming selesai.")