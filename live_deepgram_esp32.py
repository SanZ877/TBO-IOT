import asyncio
import json
import serial
import websockets

API_KEY = "6583050a882fe03f3a885048cef955c6c3069f68"
COM_PORT = "COM3"
BAUD_RATE = 921600

# 800 sampel x 2 byte (16-bit PCM) = 1600 byte per paket (50ms audio)
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

    try:
        ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=0.1)
        ser.dtr = False
        ser.rts = False

        ser.reset_input_buffer()
        print(f"Membuka port {COM_PORT} pada {BAUD_RATE} baud...")
    except Exception as e:
        print(
            f"Gagal membuka {COM_PORT}! Pastikan Serial Monitor DITUTUP. Error: {e}"
        )
        return

    async with websockets.connect(url, additional_headers=headers) as ws:
        print("Connected Deepgram")
        print("\nDEEPGRAM LIVE STT (ESP32 INMP441 USB)")
        print("Bahasa: Indonesia")
        print("Bicara sekarang...")
        print("Ctrl+C untuk berhenti.\n")

        audio_queue = asyncio.Queue()
        loop = asyncio.get_running_loop()

        # Task 1: Membaca USB Serial dan memotong tepat 1600 byte (50ms)
        def read_serial_worker():
            buffer = bytearray()
            while True:
                data = ser.read(1024)
                if data:
                    buffer.extend(data)
                    while len(buffer) >= CHUNK_SIZE:
                        chunk = bytes(buffer[:CHUNK_SIZE])
                        del buffer[:CHUNK_SIZE]
                        loop.call_soon_threadsafe(audio_queue.put_nowait, chunk)

        # Jalankan pembacaan serial di thread terpisah agar aliran konsisten
        loop.run_in_executor(None, read_serial_worker)

        # Task 2: Mengirimkan audio dari Queue ke Deepgram
        async def send_audio():
            while True:
                data = await audio_queue.get()
                await ws.send(data)

        # Task 3: Menerima hasil transkripsi
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

        # Task 4: KeepAlive
        async def keep_alive():
            while True:
                await asyncio.sleep(5)
                await ws.send(json.dumps({"type": "KeepAlive"}))

        await asyncio.gather(
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