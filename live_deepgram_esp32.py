import asyncio
import json
import socket
import websockets

API_KEY = "6583050a882fe03f3a885048cef955c6c3069f68"
UDP_IP = "0.0.0.0"  # Mendengarkan dari semua perangkat di jaringan lokal
UDP_PORT = 8888

# 800 sampel x 2 byte (16-bit PCM) = 1600 byte per paket (50ms audio - SAMA PERSIS)
CHUNK_SIZE = 1600


async def main():
    url = (
        "wss://api.deepgram.com/v1/listen"
        "?model=nova-3"
        "&language=id"
        "&encoding=linear16"
        "&sample_rate=16000"
        "&channels=1"
        "&interim_results=true"
        "&smart_format=true"
        "&punctuate=true"
    )

    headers = {"Authorization": f"Token {API_KEY}"}

    # Buka Socket UDP
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind((UDP_IP, UDP_PORT))
        sock.setblocking(False)
        print(f"UDP Receiver aktif di Port {UDP_PORT}...")
    except Exception as e:
        print(f"Gagal membuka Port {UDP_PORT}! Error: {e}")
        return

    async with websockets.connect(url, additional_headers=headers) as ws:
        print("Connected Deepgram")
        print("\nDEEPGRAM LIVE STT (ESP32 INMP441 UDP Wi-Fi)")
        print("Bahasa: Indonesia")
        print("Bicara sekarang...")
        print("Ctrl+C untuk berhenti.\n")

        audio_queue = asyncio.Queue()
        loop = asyncio.get_running_loop()

        # Task 1: Membaca UDP Socket & memotong tepat 1600 byte (50ms)
        async def read_udp_worker():
            buffer = bytearray()
            while True:
                try:
                    data, _ = await loop.sock_recvfrom(sock, 2048)
                    if data:
                        buffer.extend(data)
                        while len(buffer) >= CHUNK_SIZE:
                            chunk = bytes(buffer[:CHUNK_SIZE])
                            del buffer[:CHUNK_SIZE]
                            await audio_queue.put(chunk)
                except Exception:
                    await asyncio.sleep(0.001)

        # Task 2: Mengirimkan audio dari Queue ke Deepgram (SAMA PERSIS)
        async def send_audio():
            while True:
                data = await audio_queue.get()
                await ws.send(data)

        # Task 3: Menerima hasil transkripsi (SAMA PERSIS)
        async def receive_transcript():
            async for message in ws:
                data = json.loads(message)

                if data.get("type") != "Results":
                    continue

                channel = data.get("channel", {})
                alternatives = channel.get("alternatives", [])
                if not alternatives:
                    continue

                text = alternatives[0].get("transcript", "").strip()
                if not text:
                    continue

                if data.get("is_final"):
                    print(f"\n[FINAL]    {text}", flush=True)
                else:
                    print(f"\r[PARTIAL] {text:<100}", end="", flush=True)

        # Task 4: KeepAlive (SAMA PERSIS)
        async def keep_alive():
            while True:
                await asyncio.sleep(5)
                await ws.send(json.dumps({"type": "KeepAlive"}))

        await asyncio.gather(
            read_udp_worker(),
            send_audio(),
            receive_transcript(),
            keep_alive(),
        )


try:
    asyncio.run(main())
except KeyboardInterrupt:
    print("\nStopping...")
except Exception as e:
    print(f"\nERROR: {e}")