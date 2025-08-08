import pyaudio
import vosk
import json

model = vosk.Model(r"vosk-model-small-ru-0.22")

# Открой микрофон (вход)
mic = pyaudio.PyAudio()
mic_stream = mic.open(format=pyaudio.paInt16,
                      channels=1,
                      rate=16000,
                      input=True,
                      frames_per_buffer=8000)

mic_stream.start_stream()

rec = vosk.KaldiRecognizer(model, 16000)

print("🎤 Микрофон слушает...")

while True:
    data = mic_stream.read(4000, exception_on_overflow=False)
    if rec.AcceptWaveform(data):
        result = json.loads(rec.Result())
        print("👤 Микрофон:", result.get("text", ""))
