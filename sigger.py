import sys
import threading
import time
import numpy as np
import sounddevice as sd
import soundfile as sf
import keyboard
import customtkinter as ctk
from tkinter import filedialog


ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")


class BuffMicApp(ctk.CTk):
    def __init__(self):
        super().__init__()

        self.title("kendrk.sh")
        self.geometry("470x720")
        self.minsize(470, 720)
        self.resizable(False, False)
        self.configure(fg_color="#0b0f14")

        self.is_buffing = False
        self.is_playing_file = False
        self.stream = None
        self.devices = sd.query_devices()

        self.gain_factor = 20.0
        self.noise_level = 0.055
        self.hard_limit = 0.68
        self.use_static_effect = True

        self.input_id = None
        self.output_id = None

        self.audio_file_data = None
        self.audio_file_source_data = None
        self.audio_file_source_sr = 44100
        self.audio_file_sr = 44100
        self.file_position = 0
        self.file_name = ""
        self.stream_sample_rate = None

        self.audio_lock = threading.RLock()
        self.rng = np.random.default_rng()
        self.last_audio_error = None

        self._pulse_on = False
        self._pulse_job = None
        self._meter_value = 0.0
        self._meter_job = None

        self.setup_ui()
        self.load_devices()

        try:
            keyboard.add_hotkey("f4", self.toggle_buff_hotkey)
        except Exception:
            pass

        self.protocol("WM_DELETE_WINDOW", self.on_closing)
        self.after(80, self.update_meter)
        self.after(250, self.animate_status)

    def setup_ui(self):
        self.header = ctk.CTkFrame(
            self,
            fg_color="#10161f",
            corner_radius=18,
            border_width=1,
            border_color="#202b38",
        )
        self.header.pack(fill="x", padx=16, pady=(16, 10))

        self.lbl_title = ctk.CTkLabel(
            self.header,
            text="BUFF MIC",
            font=("Segoe UI", 23, "bold"),
            text_color="#58a6ff",
        )
        self.lbl_title.pack(anchor="w", padx=18, pady=(14, 0))

        self.lbl_subtitle = ctk.CTkLabel(
            self.header,
            text="Real-time booster",
            font=("Segoe UI", 11),
            text_color="#7d8b99",
        )
        self.lbl_subtitle.pack(anchor="w", padx=18, pady=(0, 12))

        self.status_row = ctk.CTkFrame(self, fg_color="transparent")
        self.status_row.pack(fill="x", padx=20, pady=(0, 8))

        self.lbl_status = ctk.CTkLabel(
            self.status_row,
            text="●  OFFLINE",
            font=("Segoe UI", 12, "bold"),
            text_color="#8b949e",
            fg_color="#151b23",
            corner_radius=10,
            width=130,
            height=32,
        )
        self.lbl_status.pack(side="left")

        self.lbl_meter = ctk.CTkLabel(
            self.status_row,
            text="READY",
            font=("Consolas", 10, "bold"),
            text_color="#667482",
        )
        self.lbl_meter.pack(side="right", padx=4)

        self.frame_slider = ctk.CTkFrame(
            self,
            fg_color="#111821",
            corner_radius=14,
            border_color="#202b38",
            border_width=1,
        )
        self.frame_slider.pack(fill="x", padx=20, pady=7)

        top_gain = ctk.CTkFrame(self.frame_slider, fg_color="transparent")
        top_gain.pack(fill="x", padx=15, pady=(12, 0))

        self.lbl_slider = ctk.CTkLabel(
            top_gain,
            text="BUFF LEVEL",
            font=("Segoe UI", 11, "bold"),
            text_color="#c9d1d9",
        )
        self.lbl_slider.pack(side="left")

        self.lbl_gain_value = ctk.CTkLabel(
            top_gain,
            text="100%",
            font=("Consolas", 12, "bold"),
            text_color="#58a6ff",
        )
        self.lbl_gain_value.pack(side="right")

        self.slider_gain = ctk.CTkSlider(
            self.frame_slider,
            from_=0,
            to=500,
            number_of_steps=100,
            button_color="#2f81f7",
            button_hover_color="#58a6ff",
            progress_color="#1f6feb",
            fg_color="#202938",
            command=self.update_gain,
        )
        self.slider_gain.set(100)
        self.slider_gain.pack(fill="x", padx=15, pady=(4, 3))

        self.lbl_gain_hint = ctk.CTkLabel(
            self.frame_slider,
            text="0–500%  •  extreme gain + hard clip + grit / meme distortion",
            font=("Segoe UI", 9),
            text_color="#667482",
        )
        self.lbl_gain_hint.pack(anchor="w", padx=15, pady=(0, 11))

        self.btn_toggle = ctk.CTkButton(
            self,
            text="START BUFF",
            font=("Segoe UI", 13, "bold"),
            fg_color="#238636",
            hover_color="#2ea043",
            text_color="#ffffff",
            height=46,
            corner_radius=12,
            command=self.toggle_buff,
        )
        self.btn_toggle.pack(fill="x", padx=20, pady=(2, 10))

        self.frame_file = ctk.CTkFrame(
            self,
            fg_color="#111821",
            corner_radius=14,
            border_color="#202b38",
            border_width=1,
        )
        self.frame_file.pack(fill="x", padx=20, pady=7)

        self.lbl_file_header = ctk.CTkLabel(
            self.frame_file,
            text="AUDIO FILE",
            font=("Segoe UI", 11, "bold"),
            text_color="#c9d1d9",
        )
        self.lbl_file_header.pack(anchor="w", padx=14, pady=(12, 2))

        self.lbl_filename = ctk.CTkLabel(
            self.frame_file,
            text="No file selected",
            font=("Segoe UI", 10),
            text_color="#667482",
            anchor="w",
        )
        self.lbl_filename.pack(fill="x", padx=14, pady=(0, 6))

        self.box_file_btns = ctk.CTkFrame(
            self.frame_file, fg_color="transparent"
        )
        self.box_file_btns.pack(fill="x", padx=14, pady=(0, 8))

        self.btn_select_file = ctk.CTkButton(
            self.box_file_btns,
            text="SELECT FILE",
            width=125,
            height=34,
            corner_radius=9,
            fg_color="#202938",
            hover_color="#2b3748",
            text_color="#58a6ff",
            command=self.select_audio_file,
        )
        self.btn_select_file.pack(side="left", padx=(0, 7))

        self.btn_play_file = ctk.CTkButton(
            self.box_file_btns,
            text="PLAY",
            width=100,
            height=34,
            corner_radius=9,
            fg_color="#1f6feb",
            hover_color="#388bfd",
            text_color="#ffffff",
            command=self.toggle_play_file,
        )
        self.btn_play_file.pack(side="left")

        self.chk_buff_file = ctk.CTkCheckBox(
            self.frame_file,
            text="Add static / grit effect",
            font=("Segoe UI", 10),
            text_color="#8b949e",
            fg_color="#238636",
            hover_color="#2ea043",
            command=self.on_effect_toggle,
        )
        self.chk_buff_file.pack(anchor="w", padx=14, pady=(0, 12))
        self.chk_buff_file.select()

        self.frame_devices = ctk.CTkFrame(
            self,
            fg_color="#111821",
            corner_radius=14,
            border_color="#202b38",
            border_width=1,
        )
        self.frame_devices.pack(fill="x", padx=20, pady=(7, 16))

        self.lbl_devices = ctk.CTkLabel(
            self.frame_devices,
            text="AUDIO ROUTING",
            font=("Segoe UI", 11, "bold"),
            text_color="#c9d1d9",
        )
        self.lbl_devices.pack(anchor="w", padx=14, pady=(12, 4))

        self.lbl_in = ctk.CTkLabel(
            self.frame_devices,
            text="Input / Microphone",
            font=("Segoe UI", 10),
            text_color="#7d8b99",
        )
        self.lbl_in.pack(anchor="w", padx=14)

        self.combo_input = ctk.CTkOptionMenu(
            self.frame_devices,
            height=34,
            corner_radius=9,
            fg_color="#202938",
            button_color="#2b3748",
            button_hover_color="#34445a",
            dropdown_fg_color="#151b23",
            dropdown_hover_color="#202938",
            text_color="#c9d1d9",
            command=self.on_device_change,
        )
        self.combo_input.pack(fill="x", padx=14, pady=(3, 7))

        self.lbl_out = ctk.CTkLabel(
            self.frame_devices,
            text="Output / Virtual Cable",
            font=("Segoe UI", 10),
            text_color="#7d8b99",
        )
        self.lbl_out.pack(anchor="w", padx=14)

        self.combo_output = ctk.CTkOptionMenu(
            self.frame_devices,
            height=34,
            corner_radius=9,
            fg_color="#202938",
            button_color="#2b3748",
            button_hover_color="#34445a",
            dropdown_fg_color="#151b23",
            dropdown_hover_color="#202938",
            text_color="#c9d1d9",
            command=self.on_device_change,
        )
        self.combo_output.pack(fill="x", padx=14, pady=(3, 13))

    def load_devices(self):
        input_options, output_options = [], []
        self.input_map, self.output_map = {}, {}

        for i, dev in enumerate(self.devices):
            name = f"[{i}] {dev['name']}"

            if dev["max_input_channels"] > 0:
                input_options.append(name)
                self.input_map[name] = i

            if dev["max_output_channels"] > 0:
                output_options.append(name)
                self.output_map[name] = i

        if input_options:
            self.combo_input.configure(values=input_options)
            self.combo_input.set(input_options[0])
            self.input_id = self.input_map[input_options[0]]

        if output_options:
            self.combo_output.configure(values=output_options)

            cable_default = next(
                (
                    opt
                    for opt in output_options
                    if "cable input" in opt.lower()
                    or "vb-audio" in opt.lower()
                    or "virtual" in opt.lower()
                ),
                output_options[0],
            )

            self.combo_output.set(cable_default)
            self.output_id = self.output_map[cable_default]

    def update_gain(self, value):
        val_int = int(round(float(value)))
        self.lbl_gain_value.configure(text=f"{val_int}%")

        self.gain_factor = (val_int / 100.0) * 20.0

        self.lbl_slider.configure(text="BUFF LEVEL")

    def on_effect_toggle(self):
        self.use_static_effect = bool(self.chk_buff_file.get())

    def select_audio_file(self):
        file_path = filedialog.askopenfilename(
            title="Select audio file",
            filetypes=[
                ("Audio Files", "*.wav *.mp3 *.ogg *.flac"),
                ("WAV", "*.wav"),
                ("All Files", "*.*"),
            ],
        )

        if not file_path:
            return

        try:
            data, sr = sf.read(file_path, dtype="float32")

            if data.ndim > 1:
                data = np.mean(data, axis=1)

            data = np.asarray(data, dtype=np.float32)

            data = np.nan_to_num(
                data,
                nan=0.0,
                posinf=0.0,
                neginf=0.0,
            )

            with self.audio_lock:
                self.audio_file_source_data = data
                self.audio_file_source_sr = sr
                self.audio_file_data = data
                self.audio_file_sr = sr
                self.file_position = 0
                self.file_name = file_path.rsplit("\\", 1)[-1].rsplit("/", 1)[-1]

            self.prepare_file_for_stream()

            self.lbl_filename.configure(
                text=f"✓  {self.file_name}",
                text_color="#3fb950",
            )
            self.lbl_meter.configure(text=f"{sr:,} Hz  •  READY")

        except Exception as exc:
            self.audio_file_data = None
            self.lbl_filename.configure(
                text=f"Error: {type(exc).__name__}",
                text_color="#f85149",
            )

    def prepare_file_for_stream(self, target_sr=None):
        """Resample the loaded file outside the audio callback."""
        with self.audio_lock:
            source = self.audio_file_source_data
            source_sr = self.audio_file_source_sr

        if source is None:
            return

        if target_sr is None:
            target_sr = self.get_stream_sample_rate()

        if not target_sr:
            return

        if source_sr != target_sr and len(source) > 1:
            prepared = self.resample_audio(source, source_sr, target_sr)
        else:
            prepared = source

        with self.audio_lock:
            self.audio_file_data = prepared
            self.audio_file_sr = int(target_sr)
            self.file_position = 0

    def get_stream_sample_rate(self):
        try:
            if self.input_id is None:
                return None
            info = sd.query_devices(self.input_id)
            return int(info["default_samplerate"])
        except Exception:
            return None

    @staticmethod
    def resample_audio(data, source_sr, target_sr):
        if source_sr == target_sr or len(data) < 2:
            return data

        ratio = float(target_sr) / float(source_sr)
        new_length = max(1, int(round(len(data) * ratio)))

        old_x = np.linspace(0.0, 1.0, len(data), endpoint=False)
        new_x = np.linspace(0.0, 1.0, new_length, endpoint=False)

        return np.interp(new_x, old_x, data).astype(np.float32)

    def toggle_play_file(self):
        with self.audio_lock:
            has_file = self.audio_file_data is not None

        if not has_file:
            self.lbl_filename.configure(
                text="Please select an audio file first",
                text_color="#f85149",
            )
            return

        self.is_playing_file = not self.is_playing_file

        if self.is_playing_file:
            self.btn_play_file.configure(
                text="STOP",
                fg_color="#da3633",
                hover_color="#f85149",
            )

            if not self.stream:
                self.start_stream()
        else:
            self.btn_play_file.configure(
                text="PLAY",
                fg_color="#1f6feb",
                hover_color="#388bfd",
            )

            with self.audio_lock:
                self.file_position = 0

            if not self.is_buffing:
                self.stop_stream()

    def audio_callback(self, indata, outdata, frames, time_info, status):
        if status:
            self.last_audio_error = str(status)

        mic = indata[:, :1]
        mix_data = np.zeros_like(mic)

        buffing = bool(self.is_buffing)
        playing_file = bool(self.is_playing_file)
        gain = float(self.gain_factor)
        static_enabled = bool(self.use_static_effect)

        if buffing:
            mix_data += mic

        if playing_file:
            with self.audio_lock:
                data = self.audio_file_data
                position = self.file_position

                if data is not None and len(data) > 0:
                    total = len(data)
                    end_pos = position + frames

                    if end_pos <= total:
                        file_chunk = data[position:end_pos]
                    else:
                        first = data[position:total]
                        remain = frames - len(first)
                        second = data[:remain]
                        file_chunk = np.concatenate((first, second))

                    self.file_position = end_pos % total
                else:
                    file_chunk = None

            if file_chunk is not None:
                mix_data += file_chunk.reshape(-1, 1)

        if buffing or playing_file:
            mix_data *= gain

            if static_enabled:
                noise_amount = self.noise_level * min(2.5, max(0.15, gain / 20.0))
                noise = self.rng.normal(
                    0.0,
                    noise_amount,
                    size=mix_data.shape,
                ).astype(np.float32)
                mix_data += noise

                drive = min(8.0, 1.5 + gain / 14.0)
                mix_data = np.tanh(mix_data * drive)

                crush_bits = 7 if gain < 30 else 6 if gain < 60 else 5
                levels = float(2 ** crush_bits)
                mix_data = np.round(mix_data * levels) / levels

            clip = self.hard_limit
            mix_data = np.clip(mix_data, -clip, clip)

            mix_data = np.tanh(mix_data * 4.0)

        outdata[:, :] = np.clip(mix_data, -1.0, 1.0)

        if frames:
            peak = float(np.max(np.abs(mix_data)))
            self._meter_value = min(1.0, peak)

    def start_stream(self):
        self.stop_stream()

        try:
            if self.input_id is None or self.output_id is None:
                raise RuntimeError("Input/output device is not selected.")

            in_info = sd.query_devices(self.input_id)
            sample_rate = int(in_info["default_samplerate"])
            self.stream_sample_rate = sample_rate

            self.prepare_file_for_stream(sample_rate)

            self.stream = sd.Stream(
                device=(self.input_id, self.output_id),
                samplerate=sample_rate,
                blocksize=256,
                channels=1,
                dtype="float32",
                latency="low",
                callback=self.audio_callback,
            )
            self.stream.start()

            self.lbl_status.configure(
                text="●  STREAMING",
                text_color="#3fb950",
            )
            self.lbl_meter.configure(text=f"{sample_rate:,} Hz  •  LIVE  •  GRIT")

        except Exception as exc:
            self.stream = None
            self.last_audio_error = str(exc)

            self.lbl_status.configure(
                text="●  STREAM ERROR",
                text_color="#f85149",
            )
            self.lbl_meter.configure(text="Check audio devices")

    def stop_stream(self):
        stream = self.stream
        self.stream = None

        if stream:
            try:
                stream.stop()
            except Exception:
                pass

            try:
                stream.close()
            except Exception:
                pass

    def toggle_buff_hotkey(self):
        self.after(0, self.toggle_buff)

    def toggle_buff(self):
        self.is_buffing = not self.is_buffing

        if self.is_buffing:
            self.lbl_status.configure(
                text="●  BUFF ON",
                text_color="#3fb950",
            )
            self.btn_toggle.configure(
                text="STOP BUFF",
                fg_color="#da3633",
                hover_color="#f85149",
            )

            if not self.stream:
                self.start_stream()
        else:
            self.lbl_status.configure(
                text="●  READY",
                text_color="#8b949e",
            )
            self.btn_toggle.configure(
                text="START BUFF",
                fg_color="#238636",
                hover_color="#2ea043",
            )

            if not self.is_playing_file:
                self.stop_stream()

    def on_device_change(self, choice):
        self.input_id = self.input_map.get(self.combo_input.get())
        self.output_id = self.output_map.get(self.combo_output.get())

        if self.is_buffing or self.is_playing_file:
            self.start_stream()
        else:
            self.prepare_file_for_stream()

    def animate_status(self):
        active = self.is_buffing or self.is_playing_file

        if active:
            self._pulse_on = not self._pulse_on

            self.lbl_status.configure(
                border_width=1 if self._pulse_on else 0,
                border_color="#2ea043" if self._pulse_on else "#151b23",
            )
        else:
            self.lbl_status.configure(border_width=0)

        self._pulse_job = self.after(450, self.animate_status)

    def update_meter(self):
        self._meter_value *= 0.78

        if self.is_buffing or self.is_playing_file:
            percent = int(self._meter_value * 100)
            self.lbl_meter.configure(
                text=f"AUDIO  {'█' * min(12, percent // 8)}"
                f"{'·' * max(0, 12 - percent // 8)}"
            )

        self._meter_job = self.after(80, self.update_meter)

    def on_closing(self):
        self.is_buffing = False
        self.is_playing_file = False

        try:
            keyboard.unhook_all_hotkeys()
        except Exception:
            pass

        if self._pulse_job:
            self.after_cancel(self._pulse_job)

        if self._meter_job:
            self.after_cancel(self._meter_job)

        self.stop_stream()
        self.destroy()


if __name__ == "__main__":
    app = BuffMicApp()
    app.mainloop()
