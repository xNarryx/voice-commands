import pyaudio
import vosk
import json
import threading
import time
import os

model_path = r"vosk-model-small-ru-0.22"
model = vosk.Model(model_path)

p = pyaudio.PyAudio()

# ⚠️ Укажи реальные ID микрофонов: основной и дополнительный
mic_ids = [1, 2]
threads = []

# Файл, где будет храниться режим
mode_file = "mic_mode.txt"

def recognize_from_microphone(device_index, output_file, stop_event):
    stream = p.open(format=pyaudio.paInt16,
                    channels=1,
                    rate=16000,
                    input=True,
                    input_device_index=device_index,
                    frames_per_buffer=8000)
    stream.start_stream()

    rec = vosk.KaldiRecognizer(model, 16000)

    print(f"🎧 Старт прослушки: устройство {device_index}")

    while not stop_event.is_set():
        data = stream.read(4000, exception_on_overflow=False)
        if rec.AcceptWaveform(data):
            result = json.loads(rec.Result())
            text = result.get("text", "")
            print(f"[{device_index}] {text}")
            with open(output_file, "w", encoding="utf-8") as f:
                f.write(text)

    stream.stop_stream()
    stream.close()

def start_threads(active_ids):
    stop_all_threads()
    for mic_id in active_ids:
        output_file = f"mic_{mic_id}.txt"
        stop_event = threading.Event()
        thread = threading.Thread(target=recognize_from_microphone, args=(mic_id, output_file, stop_event))
        thread.daemon = True
        thread.start()
        threads.append((thread, stop_event))

def stop_all_threads():
    for thread, event in threads:
        event.set()
    threads.clear()

def read_mode():
    if not os.path.exists(mode_file):
        return False
    with open(mode_file, "r") as f:
        return f.read().strip().lower() == "true"

# --- Основной цикл ---
last_mode = None

try:
    while True:
        current_mode = read_mode()
        if current_mode != last_mode:
            print(f"🔄 Режим изменён: {'только основной микрофон' if current_mode else 'все микрофоны'}")
            if current_mode:
                start_threads([mic_ids[0]])  # только основной
            else:
                start_threads(mic_ids)       # все микрофоны
            last_mode = current_mode
        time.sleep(10)  # Пауза, чтобы не грузить процессор
except KeyboardInterrupt:
    print("⛔ Остановка...")
    stop_all_threads()
