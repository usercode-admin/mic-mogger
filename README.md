# mic-mogger
This tool mods your microphone's sound to be extremely loud in real time. 

# 🎙️ Buff Mic (kendrk.sh)

> A real-time high-gain audio booster, soundboard, and ear-destroying distortion tool designed for funny voice chats, gaming memes, and harmless pranks.

[![Python](https://img.shields.io/badge/Python-3.8%2B-blue.svg)](https://www.python.org/)
[![UI](https://img.shields.io/badge/GUI-CustomTkinter-blueviolet.svg)](https://github.com/TomSchimansky/CustomTkinter)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

---

## 📌 Overview

**Buff Mic** is a lightweight Python application built with **CustomTkinter** and **SoundDevice**. It routes real-time microphone input and custom audio files through extreme gain amplification, dynamic distortion, bit-crushing, and noise effects before outputting to your desired audio device or virtual audio cable (e.g., VB-Audio Cable).

Whether you want to sound like a 2008 Xbox Live lobby microphone, blow up Discord call audio on command, or stream sound clips directly into voice chat, Buff Mic gives you full control over the audio chaotic intensity.

---

## ✨ Features

- **💥 Extreme Real-Time Mic Boosting:** Adjust boost level up to **500%** with extreme gain, hard clipping, and grit.
- **🔊 Soundboard / Audio File Player:** Load and stream `.wav`, `.mp3`, `.ogg`, or `.flac` files directly into your voice output channel.
- **🎛️ Custom Meme FX Engine:** Dynamic saturation (`tanh`), bit-crushing, and static noise effect toggle for authentic low-quality / distorted mic vibes.
- **🔄 Low-Latency Audio Routing:** Seamlessly select input microphones and output devices (e.g., Virtual Audio Cable).
- **⚡ Hotkey Support:** Toggle mic buffing instantly using the default global hotkey (`F4`).
- **📊 Real-time Visualizer & Meter:** Sleek dark-mode GUI featuring animated status indicators and real-time peak visualizers.

---

## 🛠️ Prerequisites & Installation

### 1. Requirements
* Python 3.8 or higher
* [VB-Audio Virtual Cable](https://vb-audio.com/Cable/) (Recommended for routing audio to Discord, Teamspeak, or in-game voice chat)

### 2. Dependencies
Install all required Python packages using pip:

```bash
pip install numpy sounddevice soundfile keyboard customtkinter
```

*Note: On Linux, ensure `portaudio` is installed (`sudo apt-get install libportaudio2`). Run with administrative privileges if hotkey detection requires root/admin rights.*

---

## 🚀 Quick Start

1. **Clone the Repository:**
   ```bash
   https://github.com/usercode-admin/mic-mogger.git
   cd mic-mogger
   ```

2. **Run the Application:**
   ```bash
   python sigger.py
   ```

3. **Configure Audio Routing:**
   - Set **Input / Microphone** to your real microphone.
   - Set **Output / Virtual Cable** to your **CABLE Input (VB-Audio Virtual Cable)**.
   - In Discord/Game settings, set your **Input Device** to **CABLE Output (VB-Audio Virtual Cable)**.

4. **Have Fun:**
   - Press **F4** or click **START BUFF** to boost your mic.
   - Adjust the **BUFF LEVEL** slider to control distortion intensity.
   - Load custom meme sounds via **SELECT FILE** and hit **PLAY**.

---

## ⚙️ Audio Pipeline Architecture

```text
[ Mic Input / Audio File ] ──► [ Gain Multiplier (0-500%) ] ──► [ Static / Noise Generator ] 
                                                                             │
[ Virtual Cable Output ] ◄── [ Hard Limiter & Clip ] ◄── [ Bit-crusher & Saturation ]
```

---

## ⚠️ Disclaimer & Usage Policy

This project was created purely for **educational, entertainment, and comedic purposes**. 

* **Ears Safety Warning:** High gain levels and distortion can produce loud noises. Use with care when using headphones or near high-volume speakers.
* **Community Guidelines:** Respect others in online voice channels. Avoid using this tool to intentionally harass, harm, or disrupt users.

---

## 📜 License

This project is open-source under the **MIT License**.
