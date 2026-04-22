#!/usr/bin/env python3
"""
Generate clean synthetic IRs for Premonition: Hall, Plate, Spring, Room.
Uses exponentially decaying filtered noise with per-type shaping.
Output: 44.1 kHz stereo 24-bit PCM WAV, dithered clean, tail-faded.
"""
import numpy as np
from scipy import signal
import soundfile as sf
import os

SR = 44100
OUT = "/Users/aviouslyavi/Claude/Projects/premonition/Premonition/resources/ir"

def decorrelate_stereo(mono_ir, max_delay_samples=23, decorr_factor=0.15):
    """Derive R channel from L via short delay + slight spectral shift for stereo width."""
    L = mono_ir.copy()
    R = mono_ir.copy()
    # Short comb delay on R for Haas-ish width
    d = np.zeros(max_delay_samples)
    R = np.concatenate([d, R[:-max_delay_samples]])
    # Slight amplitude modulation for decorrelation
    noise = np.random.RandomState(42).randn(len(R)) * decorr_factor
    R = R * (1.0 + noise * np.exp(-np.arange(len(R)) / (SR * 0.05)))
    return L, R

def exponential_decay_noise(duration_s, rt60_s, early_reflect_boost=0.0,
                             lpf_hz=None, hpf_hz=None, seed=1):
    n = int(duration_s * SR)
    rng = np.random.RandomState(seed)
    noise = rng.randn(n).astype(np.float32)
    # Exponential decay: g(t) = 10^(-3t/RT60)
    t = np.arange(n) / SR
    env = np.power(10.0, -3.0 * t / rt60_s).astype(np.float32)
    ir = noise * env
    # Optional early-reflection spike (first few ms)
    if early_reflect_boost > 0:
        er_len = int(0.01 * SR)  # 10ms
        er_env = np.exp(-np.arange(er_len) / (SR * 0.002))
        ir[:er_len] += rng.randn(er_len) * er_env * early_reflect_boost
    # Band-limit
    if lpf_hz and lpf_hz < SR / 2:
        b, a = signal.butter(4, lpf_hz / (SR / 2), btype='low')
        ir = signal.filtfilt(b, a, ir).astype(np.float32)
    if hpf_hz and hpf_hz > 20:
        b, a = signal.butter(2, hpf_hz / (SR / 2), btype='high')
        ir = signal.filtfilt(b, a, ir).astype(np.float32)
    return ir

def make_hall():
    # Long, dark, diffuse
    return exponential_decay_noise(duration_s=3.5, rt60_s=2.8,
                                    early_reflect_boost=0.6,
                                    lpf_hz=8000, hpf_hz=80, seed=1)

def make_plate():
    # Medium, bright, dense
    return exponential_decay_noise(duration_s=2.2, rt60_s=1.8,
                                    early_reflect_boost=0.8,
                                    lpf_hz=12000, hpf_hz=120, seed=2)

def make_spring():
    # Short, resonant, metallic — add chirp-like resonances
    base = exponential_decay_noise(duration_s=1.6, rt60_s=1.2,
                                    early_reflect_boost=1.0,
                                    lpf_hz=6000, hpf_hz=200, seed=3)
    # Add a few damped sine resonances for that spring sound
    t = np.arange(len(base)) / SR
    for f0, decay, amp in [(1200, 0.4, 0.3), (2400, 0.3, 0.15), (3800, 0.25, 0.08)]:
        base += (amp * np.sin(2 * np.pi * f0 * t) *
                 np.exp(-t / decay)).astype(np.float32)
    return base

def make_room():
    # Short, tight, bright
    return exponential_decay_noise(duration_s=0.8, rt60_s=0.5,
                                    early_reflect_boost=1.2,
                                    lpf_hz=14000, hpf_hz=100, seed=4)

def tail_fade(ir, fade_s=0.15):
    n_fade = int(fade_s * SR)
    if n_fade > len(ir):
        n_fade = len(ir) // 4
    fade = np.cos(np.linspace(0, np.pi / 2, n_fade)).astype(np.float32)
    ir[-n_fade:] *= fade
    return ir

def write_ir(name, mono_ir):
    # Normalize to -1 dBFS peak
    peak = np.max(np.abs(mono_ir))
    if peak > 0:
        mono_ir = mono_ir * (0.891 / peak)
    # Stereo decorrelate
    L, R = decorrelate_stereo(mono_ir)
    # Equalize lengths
    n = min(len(L), len(R))
    L, R = L[:n], R[:n]
    # Final tail fade
    L = tail_fade(L)
    R = tail_fade(R)
    # Clean denormals
    L[np.abs(L) < 1e-20] = 0.0
    R[np.abs(R) < 1e-20] = 0.0
    stereo = np.stack([L, R], axis=1).astype(np.float32)
    # Final peak normalize to -1 dBFS (fixes any post-decorrelation overshoot)
    final_peak = np.max(np.abs(stereo))
    if final_peak > 0:
        stereo = (stereo * (0.891 / final_peak)).astype(np.float32)
    out = os.path.join(OUT, f"{name}.wav")
    sf.write(out, stereo, SR, subtype='FLOAT')
    print(f"  {name}.wav: {n} samples ({n/SR:.2f}s), peak={np.max(np.abs(stereo)):.3f}, "
          f"RMS={20*np.log10(np.sqrt(np.mean(stereo**2))):.1f} dB, "
          f"denormals={(np.abs(stereo) < 1.2e-38).sum() - (stereo == 0).sum()}")

print("Generating clean IRs...")
write_ir("hall",   make_hall())
write_ir("plate",  make_plate())
write_ir("spring", make_spring())
write_ir("room",   make_room())
print("Done.")
