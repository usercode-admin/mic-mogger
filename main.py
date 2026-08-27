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
ctk.set_default_color_theme("dark-blue")


class BuffMicApp(ctk.CTk):
    def __init__(self):
        super().__init__()

        self.title("MIGGER")
        self.geometry("560x860")
        self.minsize(560, 860)
        self.resizable(False, False)
        self.configure(fg_color="#030303")

        # UI-only visual system: monochrome glass / soft depth.
        self.ui_bg = "#030303"
        self.ui_surface = "#0b0b0b"
        self.ui_surface_2 = "#101010"
        self.ui_border = "#242424"
        self.ui_border_soft = "#1a1a1a"
        self.ui_text = "#f5f5f5"
        self.ui_muted = "#888888"
        self.ui_dim = "#5d5d5d"
        self.ui_accent = "#ffffff"

        self.is_buffing = False
        self.is_playing_file = False
        self.stream = None
        self.devices = sd.query_devices()

        self.gain_factor = 1.0
        self.noise_level = 0.012
        self.hard_limit = 0.985
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

        # Audio dynamics state (kept between callback blocks to prevent
        # volume pumping when a loud transient appears).
        self._comp_env = 0.0
        self._comp_gain = 1.0

        self.setup_ui()
        self.load_devices()

        try:
            keyboard.add_hotkey("f4", self.toggle_buff_hotkey)
        except Exception:
            pass

        self.protocol("WM_DELETE_WINDOW", self.on_closing)
        self.after(80, self.update_meter)
        self.after(250, self.animate_status)
        self.after(700, self.animate_title)

    def setup_ui(self):
        self.grid_columnconfigure(0, weight=1)

        # Top glass header
        self.header = ctk.CTkFrame(
            self,
            fg_color="#0a0a0a",
            corner_radius=22,
            border_width=1,
            border_color="#252525",
        )
        self.header.pack(fill="x", padx=18, pady=(18, 10))

        top = ctk.CTkFrame(self.header, fg_color="transparent")
        top.pack(fill="x", padx=18, pady=(15, 5))

        brand = ctk.CTkFrame(top, fg_color="transparent")
        brand.pack(side="left")

        self.lbl_title = ctk.CTkLabel(
            brand,
            text="BUFF MIC",
            font=("Segoe UI", 25, "bold"),
            text_color="#f5f5f5",
        )
        self.lbl_title.pack(anchor="w")

        self.lbl_subtitle = ctk.CTkLabel(
            brand,
            text="MIC-MOGGER",
            font=("Segoe UI", 9, "bold"),
            text_color="#777777",
        )
        self.lbl_subtitle.pack(anchor="w", pady=(1, 0))

        self.header_badge = ctk.CTkLabel(
            top,
            text="● LIVE",
            font=("Segoe UI", 9, "bold"),
            text_color="#a8a8a8",
            fg_color="#151515",
            corner_radius=10,
            padx=11,
            pady=7,
        )
        self.header_badge.pack(side="right", pady=2)

        self.header_divider = ctk.CTkFrame(
            self.header, height=1, fg_color="#202020", corner_radius=0
        )
        self.header_divider.pack(fill="x", padx=18, pady=(8, 0))

        # Status / meter strip
        self.status_row = ctk.CTkFrame(self, fg_color="transparent")
        self.status_row.pack(fill="x", padx=20, pady=(2, 8))

        self.lbl_status = ctk.CTkLabel(
            self.status_row,
            text="● OFFLINE",
            font=("Segoe UI", 10, "bold"),
            text_color="#8f8f8f",
            fg_color="#101010",
            corner_radius=10,
            width=132,
            height=31,
        )
        self.lbl_status.pack(side="left")

        self.lbl_meter = ctk.CTkLabel(
            self.status_row,
            text="START",
            font=("Consolas", 10, "bold"),
            text_color="#666666",
        )
        self.lbl_meter.pack(side="right", padx=4)

        # Main control card
        self.frame_slider = ctk.CTkFrame(
            self,
            fg_color="#0c0c0c",
            corner_radius=18,
            border_width=1,
            border_color="#252525",
        )
        self.frame_slider.pack(fill="x", padx=20, pady=6)

        top_gain = ctk.CTkFrame(self.frame_slider, fg_color="transparent")
        top_gain.pack(fill="x", padx=16, pady=(14, 0))

        self.lbl_slider = ctk.CTkLabel(
            top_gain,
            text="BUFF LEVEL",
            font=("Segoe UI", 10, "bold"),
            text_color="#e7e7e7",
        )
        self.lbl_slider.pack(side="left")

        self.lbl_gain_value = ctk.CTkLabel(
            top_gain,
            text="100%",
            font=("Consolas", 15, "bold"),
            text_color="#ffffff",
        )
        self.lbl_gain_value.pack(side="right")

        self.slider_gain = ctk.CTkSlider(
            self.frame_slider,
            from_=50,
            to=800,
            number_of_steps=150,
            button_color="#ffffff",
            button_hover_color="#cfcfcf",
            progress_color="#e9e9e9",
            fg_color="#262626",
            command=self.update_gain,
        )
        self.slider_gain.set(100)
        self.slider_gain.pack(fill="x", padx=16, pady=(7, 4))

        self.lbl_gain_hint = ctk.CTkLabel(
            self.frame_slider,
            text="50–800% - REAL TIME",
            font=("Segoe UI", 8),
            text_color="#626262",
        )
        self.lbl_gain_hint.pack(anchor="w", padx=16, pady=(0, 12))

        # Large primary action
        self.btn_toggle = ctk.CTkButton(
            self,
            text="START BUFF",
            font=("Segoe UI", 13, "bold"),
            fg_color="#f0f0f0",
            hover_color="#ffffff",
            text_color="#050505",
            height=54,
            corner_radius=16,
            command=self.toggle_buff,
        )
        self.btn_toggle.pack(fill="x", padx=20, pady=(4, 12))

        # File card
        self.frame_file = ctk.CTkFrame(
            self,
            fg_color="#0c0c0c",
            corner_radius=18,
            border_color="#252525",
            border_width=1,
        )
        self.frame_file.pack(fill="x", padx=20, pady=6)

        file_head = ctk.CTkFrame(self.frame_file, fg_color="transparent")
        file_head.pack(fill="x", padx=15, pady=(13, 2))

        self.lbl_file_header = ctk.CTkLabel(
            file_head,
            text="AUDIO FILE",
            font=("Segoe UI", 10, "bold"),
            text_color="#e7e7e7",
        )
        self.lbl_file_header.pack(side="left")

        ctk.CTkLabel(
            file_head,
            text="LOOP / PLAYBACK",
            font=("Consolas", 8, "bold"),
            text_color="#555555",
        ).pack(side="right")

        self.lbl_filename = ctk.CTkLabel(
            self.frame_file,
            text="No file selected",
            font=("Segoe UI", 10),
            text_color="#666666",
            anchor="w",
        )
        self.lbl_filename.pack(fill="x", padx=15, pady=(2, 9))

        self.box_file_btns = ctk.CTkFrame(
            self.frame_file, fg_color="transparent"
        )
        self.box_file_btns.pack(fill="x", padx=15, pady=(0, 9))

        self.btn_select_file = ctk.CTkButton(
            self.box_file_btns,
            text="SELECT FILE",
            width=132,
            height=36,
            corner_radius=10,
            fg_color="#222222",
            hover_color="#303030",
            text_color="#e8e8e8",
            command=self.select_audio_file,
        )
        self.btn_select_file.pack(side="left", padx=(0, 8))

        self.btn_play_file = ctk.CTkButton(
            self.box_file_btns,
            text="PLAY",
            width=105,
            height=36,
            corner_radius=10,
            fg_color="#eeeeee",
            hover_color="#ffffff",
            text_color="#050505",
            command=self.toggle_play_file,
        )
        self.btn_play_file.pack(side="left")

        self.chk_buff_file = ctk.CTkCheckBox(
            self.frame_file,
            text="Add static / grit effect",
            font=("Segoe UI", 9),
            text_color="#8b8b8b",
            fg_color="#ffffff",
            hover_color="#d0d0d0",
            checkmark_color="#000000",
            border_color="#666666",
            border_width=2,
            command=self.on_effect_toggle,
        )
        self.chk_buff_file.pack(anchor="w", padx=15, pady=(0, 13))
        self.chk_buff_file.select()

        # Routing card
        self.frame_devices = ctk.CTkFrame(
            self,
            fg_color="#0c0c0c",
            corner_radius=18,
            border_color="#252525",
            border_width=1,
        )
        self.frame_devices.pack(fill="x", padx=20, pady=(6, 18))

        route_head = ctk.CTkFrame(self.frame_devices, fg_color="transparent")
        route_head.pack(fill="x", padx=15, pady=(13, 4))

        self.lbl_devices = ctk.CTkLabel(
            route_head,
            text="AUDIO ROUTING",
            font=("Segoe UI", 10, "bold"),
            text_color="#e7e7e7",
        )
        self.lbl_devices.pack(side="left")

        ctk.CTkLabel(
            route_head,
            text="I/O MATRIX",
            font=("Consolas", 8, "bold"),
            text_color="#555555",
        ).pack(side="right")

        self.lbl_in = ctk.CTkLabel(
            self.frame_devices,
            text="INPUT / MICROPHONE",
            font=("Segoe UI", 8, "bold"),
            text_color="#777777",
        )
        self.lbl_in.pack(anchor="w", padx=15)

        self.combo_input = ctk.CTkOptionMenu(
            self.frame_devices,
            height=36,
            corner_radius=10,
            fg_color="#1d1d1d",
            button_color="#292929",
            button_hover_color="#383838",
            dropdown_fg_color="#111111",
            dropdown_hover_color="#292929",
            text_color="#d9d9d9",
            command=self.on_device_change,
        )
        self.combo_input.pack(fill="x", padx=15, pady=(4, 9))

        self.lbl_out = ctk.CTkLabel(
            self.frame_devices,
            text="OUTPUT / VIRTUAL CABLE",
            font=("Segoe UI", 8, "bold"),
            text_color="#777777",
        )
        self.lbl_out.pack(anchor="w", padx=15)

        self.combo_output = ctk.CTkOptionMenu(
            self.frame_devices,
            height=36,
            corner_radius=10,
            fg_color="#1d1d1d",
            button_color="#292929",
            button_hover_color="#383838",
            dropdown_fg_color="#111111",
            dropdown_hover_color="#292929",
            text_color="#d9d9d9",
            command=self.on_device_change,
        )
        self.combo_output.pack(fill="x", padx=15, pady=(4, 14))

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

        # 100% = unity gain. 800% is strong, but the compressor/limiter
        # prevents the signal from turning into a crushed, quieter mess.
        self.gain_factor = val_int / 100.0

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
                text_color="#f2f2f2",
            )
            self.lbl_meter.configure(text=f"{sr:,} Hz  •  READY")

        except Exception as exc:
            self.audio_file_data = None
            self.lbl_filename.configure(
                text=f"Error: {type(exc).__name__}",
                text_color="#f5f5f5",
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
                text_color="#f5f5f5",
            )
            return

        self.is_playing_file = not self.is_playing_file

        if self.is_playing_file:
            self.btn_play_file.configure(
                text="STOP",
                fg_color="#3b3b3b",
                hover_color="#ffffff",
            )

            if not self.stream:
                self.start_stream()
        else:
            self.btn_play_file.configure(
                text="PLAY",
                fg_color="#d8d8d8",
                hover_color="#ffffff",
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
            # Apply the selected boost first.
            x = mix_data * gain

            # Gentle high-pass / rumble reduction.
            if not hasattr(self, "_hp_prev"):
                self._hp_prev = 0.0
            hp_prev = float(np.asarray(self._hp_prev).reshape(-1)[0])
            hp = np.empty_like(x)
            hp_alpha = 0.995
            hp[0, 0] = hp_alpha * (hp_prev + x[0, 0] - hp_prev)
            if len(x) > 1:
                hp[1:, 0] = hp_alpha * (hp[1:, 0] - x[:-1, 0]) + x[1:, 0]
            self._hp_prev = float(x[-1, 0])
            x = x - hp * 0.06

            # -----------------------------------------------------------------
            # STABLE COMPRESSOR
            # -----------------------------------------------------------------
            # The old code compressed each block independently and then used
            # a block peak limiter. A single loud syllable could therefore
            # make the entire 256-sample block quieter, causing "luc luc".
            #
            # This compressor keeps an envelope and gain state across blocks,
            # so loud/quiet speech changes smoothly instead of pumping.
            threshold = 0.38
            ratio = 3.8

            sample_rate = float(self.stream_sample_rate or 48000)
            attack = max(0.001, 0.010)   # 10 ms
            release = max(0.010, 0.120)  # 120 ms
            attack_coeff = np.exp(-1.0 / (sample_rate * attack))
            release_coeff = np.exp(-1.0 / (sample_rate * release))

            env = float(self._comp_env)
            comp_gain = float(self._comp_gain)
            compressed = np.empty_like(x)

            for i in range(len(x)):
                level = abs(float(x[i, 0]))

                if level > env:
                    env = attack_coeff * env + (1.0 - attack_coeff) * level
                else:
                    env = release_coeff * env + (1.0 - release_coeff) * level

                if env > threshold:
                    desired_gain = (threshold / max(env, 1e-6)) ** (1.0 - 1.0 / ratio)
                else:
                    desired_gain = 1.0

                # Smooth gain changes too, avoiding audible pumping.
                if desired_gain < comp_gain:
                    comp_gain = attack_coeff * comp_gain + (1.0 - attack_coeff) * desired_gain
                else:
                    comp_gain = release_coeff * comp_gain + (1.0 - release_coeff) * desired_gain

                compressed[i, 0] = x[i, 0] * comp_gain

            self._comp_env = env
            self._comp_gain = comp_gain
            x = compressed

            # Controlled makeup. Higher buff settings get a little more
            # average loudness without multiplying peaks uncontrollably.
            makeup = 1.0 + min(0.85, max(0.0, np.log10(max(gain, 1.0)) * 0.65))
            x *= makeup

            if static_enabled:
                # Keep grit/noise subtle so the processor sounds loud rather
                # than merely noisy.
                noise_amount = self.noise_level * min(1.0, max(0.10, gain * 0.22))
                noise = self.rng.normal(
                    0.0,
                    noise_amount,
                    size=x.shape,
                ).astype(np.float32)
                x += noise

                # Saturation adds character but is deliberately kept moderate.
                drive = 1.0 + min(2.2, max(0.0, gain - 1.0) * 0.22)
                x = np.tanh(x * drive) / np.tanh(drive)

                if gain >= 5.0:
                    levels = 2 ** 8
                    x = np.round(x * levels) / levels

            # -----------------------------------------------------------------
            # FIXED MASTER LIMITER
            # -----------------------------------------------------------------
            # Do NOT normalize each block to its own peak. That was the main
            # source of volume pumping. Instead, use a fixed ceiling with
            # soft saturation before the final hard safety clamp.
            ceiling = 0.96
            soft_threshold = 0.82

            ax = np.abs(x)
            over = ax > soft_threshold
            if np.any(over):
                excess = ax[over] - soft_threshold
                x[over] = np.sign(x[over]) * (
                    soft_threshold + np.tanh(excess * 3.0) * (ceiling - soft_threshold)
                )

            x = np.clip(x, -ceiling, ceiling)
            mix_data = x

        outdata[:, :] = np.clip(mix_data, -1.0, 1.0)

        if frames:
            peak = float(np.max(np.abs(mix_data)))
            self._meter_value = min(1.0, peak)
    def start_stream(self):
        self.stop_stream()
        self._hp_prev = 0.0
        self._comp_env = 0.0
        self._comp_gain = 1.0

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
                text_color="#f2f2f2",
            )
            self.lbl_meter.configure(text=f"{sample_rate:,} Hz  •  LIVE  •  GRIT")

        except Exception as exc:
            self.stream = None
            self.last_audio_error = str(exc)

            self.lbl_status.configure(
                text="●  STREAM ERROR",
                text_color="#f5f5f5",
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
                text_color="#f2f2f2",
            )
            self.btn_toggle.configure(
                text="STOP BUFF",
                fg_color="#3b3b3b",
                hover_color="#ffffff",
            )

            if not self.stream:
                self.start_stream()
        else:
            self.lbl_status.configure(
                text="●  READY",
                text_color="#8f8f8f",
            )
            self.btn_toggle.configure(
                text="START BUFF",
                fg_color="#eeeeee",
                hover_color="#ffffff",
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

    def animate_title(self):
        # Small animated accent keeps the UI alive without distracting from audio.
        if self.is_buffing:
            self.lbl_title.configure(text_color="#ffffff")
        else:
            self.lbl_title.configure(text_color="#d8d8d8")
        self.after(700, self.animate_title)

    def animate_status(self):
        active = self.is_buffing or self.is_playing_file

        if active:
            self._pulse_on = not self._pulse_on

            self.lbl_status.configure(
                border_width=1 if self._pulse_on else 0,
                border_color="#ffffff" if self._pulse_on else "#181818",
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
