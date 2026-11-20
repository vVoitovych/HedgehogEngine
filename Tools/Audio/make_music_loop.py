"""Writes Projects/FeatureTest/Assets/Audio/MusicLoop.wav: a 20 s loop of four sine chords.

Mono, 16-bit, 22050 Hz. Each chord fades in and out over its own 2.5 s, so the file starts and
ends in silence and loops without a click. Run from the repository root:

    python Tools/Audio/make_music_loop.py
"""

import math
import struct
import wave

RATE = 22050
CHORD_SECONDS = 2.5
FADE_SECONDS = 0.05
AMPLITUDE = 0.2  # per note, so a three-note chord stays below full scale

# C major, A minor, F major, G major, twice: 8 chords of 2.5 s.
CHORDS = [
    (261.63, 329.63, 392.00),
    (220.00, 261.63, 329.63),
    (174.61, 220.00, 261.63),
    (196.00, 246.94, 293.66),
] * 2
OUTPUT = "Projects/FeatureTest/Assets/Audio/MusicLoop.wav"


def chord_samples(frequencies):
    count = int(RATE * CHORD_SECONDS)
    fade = int(RATE * FADE_SECONDS)
    samples = []
    for i in range(count):
        t = i / RATE
        envelope = min(1.0, i / fade, (count - 1 - i) / fade)
        value = sum(math.sin(2.0 * math.pi * f * t) for f in frequencies) * AMPLITUDE * envelope
        samples.append(int(max(-1.0, min(1.0, value)) * 32767))
    return samples


def main():
    samples = []
    for chord in CHORDS:
        samples.extend(chord_samples(chord))
    with wave.open(OUTPUT, "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(struct.pack("<%dh" % len(samples), *samples))
    print("Wrote %s: %.1f s" % (OUTPUT, len(samples) / RATE))


if __name__ == "__main__":
    main()
