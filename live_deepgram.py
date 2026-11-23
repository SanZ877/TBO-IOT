import os
import sys
import asyncio
import json
import websockets
import sounddevice as sd
import arabic_reshaper
from bidi.algorithm import get_display

# Pastikan terminal mendukung output karakter Arab (UTF-8)
sys.stdout.reconfigure(encoding="utf-8")

# Segera ganti/regenerate API key ini di dashboard Deepgram setelah selesai testing
API_KEY = "6583050a882fe03f3a885048cef955c6c3069f68"

SAMPLE_RATE = 16000
DEVICE = 1


def format_arab(teks: str) -> str:
    """Menyambung huruf Arab dan membalik arah baca menjadi Right-to-Left (RTL) untuk terminal."""
    reshaped = arabic_reshaper.reshape(teks)
    return get_display(reshaped)


async def main():
    url = (
        "wss://api.deepgram.com/v1/listen"
        "?model=nova-3"
        "&language=ar"
        "&encoding=linear16"
        "&sample_rate=16000"
        "&channels=1"
        "&interim_results=true"
    )

    headers = {
        "Authorization": f"Token {API_KEY}"
    }

    async with websockets.connect(
        url,
        additional_headers=headers,
    ) as ws:

        print("Connected Deepgram")
        print("")
        print("DEEPGRAM LIVE STT")
        print("Bahasa: Arab (Al-Qur'an / Fusha)")
        print("Mulai membaca...")
        print("Ctrl+C untuk berhenti.")
        print("")

        loop = asyncio.get_running_loop()
        audio_queue = asyncio.Queue()

        def audio_callback(indata, frames, time_info, status):
            if status:
                print(f"\nAUDIO: {status}")

            data = bytes(indata)
            loop.call_soon_threadsafe(
                audio_queue.put_nowait,
                data
            )

        async def send_audio():
            with sd.RawInputStream(
                samplerate=SAMPLE_RATE,
                channels=1,
                dtype="int16",
                blocksize=800,
                device=DEVICE,
                callback=audio_callback,
            ):
                while True:
                    data = await audio_queue.get()
                    await ws.send(data)

        async def receive_transcript():
            async for message in ws:
                data = json.loads(message)

                if data.get("type") != "Results":
                    continue

                channel = data.get("channel", {})
                alternatives = channel.get("alternatives", [])

                if not alternatives:
                    continue

                # String Arab asli (disimpan bersih untuk pencocokan ayat/database)
                raw_text = alternatives[0].get("transcript", "").strip()

                if not raw_text:
                    continue

                # String Arab yang sudah diformat khusus untuk tampilan terminal
                display_text = format_arab(raw_text)

                if data.get("is_final"):
                    print(
                        f"\r[FINAL]   {display_text:<65}",
                        flush=True
                    )
                else:
                    print(
                        f"\r[PARTIAL] {display_text:<65}",
                        end="",
                        flush=True
                    )

        async def keep_alive():
            while True:
                await asyncio.sleep(5)
                await ws.send(json.dumps({
                    "type": "KeepAlive"
                }))

        await asyncio.gather(
            send_audio(),
            receive_transcript(),
            keep_alive(),
        )


try:
    asyncio.run(main())

except KeyboardInterrupt:
    print("\nStopping...")

except websockets.exceptions.InvalidStatus as e:
    print(f"\nERROR HTTP {e.response.status_code}: {e.response.headers.get('dg-error', e.response.body)}")

except Exception as e:
    print(f"\nERROR: {e}")