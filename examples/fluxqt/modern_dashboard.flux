# Modern FluxQt Audio Dashboard
# Demonstrates: enum, match, class, generics [T], elif chains,
#               bare return, struct constructors, impl methods,
#               try/catch, struct params
# Requires: fluxqt bridge (viospice)

# ── Enums ───────────────────────────────────────────────────────────────────

enum SignalType {
    Sine,
    Square,
    Noise,
    Sweep
}

enum FilterMode {
    LowPass { cutoff: Double, order: Double },
    HighPass { cutoff: Double, order: Double },
    BandPass { f_low: Double, f_high: Double },
    Notch { f_notch: Double, q: Double }
}

enum AudioError {
    Clip { channel: Double, peak: Double },
    Overload { temp: Double },
    NoInput
}

# ── Structs ─────────────────────────────────────────────────────────────────

struct MeterReading {
    rms: Double
    peak: Double
    clip_count: Double
}

struct BandInfo {
    name: Double
    freq: Double
    desc: Double
}

# ── Generic utilities ───────────────────────────────────────────────────────

def clamp[T](x: T, lo: T, hi: T) -> T {
    if (x < lo) { lo }
    elif (x > hi) { hi }
    else { x }
}

def map_range[T](x: T, in_lo: T, in_hi: T, out_lo: T, out_hi: T) -> T {
    t = (x - in_lo) / (in_hi - in_lo)
    out_lo + (out_hi - out_lo) * clamp(t, 0.0, 1.0)
}

# ── Class ───────────────────────────────────────────────────────────────────

class ChannelStrip {
    gain: Double
    pan: Double
    mute: Double
    solo: Double

    def apply_gain(self, sample: Double) -> Double {
        if (self.mute != 0.0) {
            return 0.0
        }
        sample * self.gain
    }

    def apply_pan(self, sample: Double) -> Double {
        if (self.pan < -0.5) {
            sample * 0.5
        } elif (self.pan > 0.5) {
            sample * -0.5
        } else {
            sample * (1.0 - self.pan * self.pan)
        }
    }

    def process(self, sample: Double) -> Double {
        wet = self.apply_gain(sample)
        self.apply_pan(wet)
    }
}

class MasterBus {
    channels: Double
    level: Double

    def sum(self, chan_a: Double, chan_b: Double) -> Double {
        raw = chan_a + chan_b
        if (raw > 1.0) {
            1.0
        } elif (raw < -1.0) {
            -1.0
        } else {
            raw
        }
    }
}

# ── Impl for MeterReading ───────────────────────────────────────────────────

impl MeterReading {
    def is_clipping(self) -> Double {
        self.peak >= 1.0 || self.clip_count > 0.0
    }

    def label(self) -> Double {
        if (self.is_clipping() != 0.0) {
            "CLIP"
        } elif (self.rms > -6.0) {
            "Hot"
        } elif (self.rms > -18.0) {
            "Nominal"
        } else {
            "Low"
        }
    }
}

# ── Match-based signal description ──────────────────────────────────────────

def describe_signal(sig: SignalType) -> Double {
    match sig {
        SignalType.Sine -> "Sine wave",
        SignalType.Square -> "Square wave",
        SignalType.Noise -> "White Noise",
        SignalType.Sweep -> "Frequency sweep",
        default -> "Unknown"
    }
}

# ── Filter description with elif chain ──────────────────────────────────────

def describe_filter(mode: FilterMode) -> Double {
    match mode {
        FilterMode.LowPass -> "Low-pass filter",
        FilterMode.HighPass -> "High-pass filter",
        FilterMode.BandPass -> "Band-pass filter",
        FilterMode.Notch -> "Notch filter",
        default -> "No filter"
    }
}

# ── Frequency description (elif chain, 7-way) ───────────────────────────────

def describe_freq(fc: Double) -> Double {
    if (fc < 20.0) {
        "Subsonic"
    } elif (fc < 250.0) {
        "Bass"
    } elif (fc < 500.0) {
        "Low Mid"
    } elif (fc < 2000.0) {
        "Mid"
    } elif (fc < 6000.0) {
        "High Mid"
    } elif (fc < 20000.0) {
        "Treble"
    } else {
        "Ultrasonic"
    }
}

# ── Gain stage with elif chain ──────────────────────────────────────────────

def gain_stage_db(val: Double) -> Double {
    if (val < 10.0) {
        -18.0
    } elif (val < 30.0) {
        -6.0
    } elif (val < 60.0) {
        0.0
    } elif (val < 85.0) {
        6.0
    } else {
        12.0
    }
}

# ── Try/catch around a dangerous operation ──────────────────────────────────

def try_process(sample: Double) -> Double {
    try {
        raw = sample * 100.0
        if (raw > 1.0) {
            throw 1.0
        } elif (raw < -1.0) {
            throw -1.0
        }
        raw
    } catch e {
        0.0
    }
}

# ── Error description with match ────────────────────────────────────────────

def describe_error(err: AudioError) -> Double {
    match err {
        AudioError.Clip -> "Clipping detected",
        AudioError.Overload -> "Thermal overload",
        AudioError.NoInput -> "No input signal",
        default -> "Unknown error"
    }
}

# ── Function taking a struct parameter ──────────────────────────────────────

def format_reading(m: MeterReading) -> Double {
    flux_concat(m.label(),
        flux_concat("  ",
            flux_concat(flux_to_str(m.rms),
                flux_concat("dB  peak ",
                    flux_concat(flux_to_str(m.peak),
                        flux_concat("  clips ",
                            flux_to_str(m.clip_count)))))))
}

# ── Bare return for validation ──────────────────────────────────────────────

def validate_gain(val: Double) -> Double {
    if (val == 0.0) {
        return 0.0
    }
    if (val < -12.0) {
        flux_qt_msg_box("Warning", "Gain below -12 dB")
        return 0.0
    } elif (val > 12.0) {
        flux_qt_msg_box("Warning", "Gain above +12 dB")
        return 0.0
    }
    val
}

# ── Widget signal handlers ──────────────────────────────────────────────────

def on_signal_changed() {
    combo_h = flux_get_var("SIG_COMBO")
    idx = combo_h.currentIndex

    desc = ""

    if (idx == 0.0) {
        desc = describe_signal(SignalType.Sine)
    } elif (idx == 1.0) {
        desc = describe_signal(SignalType.Square)
    } elif (idx == 2.0) {
        desc = describe_signal(SignalType.Noise)
    } elif (idx == 3.0) {
        desc = describe_signal(SignalType.Sweep)
    } else {
        return
    }

    info_h = flux_get_var("SIG_INFO")
    info_h.text = desc
}

def on_gain_changed() {
    gain_s = flux_get_var("GAIN_SLIDER")
    gval = clamp(gain_s.value, 0.0, 100.0)

    bar_h = flux_get_var("GAIN_BAR")
    bar_h.value = gval

    lcd_h = flux_get_var("GAIN_LCD")
    lcd_h.display = gain_stage_db(gval)

    # Apply via class instance stored in global
    ch = flux_get_var("CHANNEL_STRIP")
    ch.gain = clamp(map_range(gval, 0.0, 100.0, 0.0, 2.0), 0.0, 2.0)
}

def on_pan_changed() {
    pan_s = flux_get_var("PAN_SLIDER")
    pval = clamp(pan_s.value, -1.0, 1.0)

    ch = flux_get_var("CHANNEL_STRIP")
    ch.pan = pval

    lcd_h = flux_get_var("PAN_LCD")
    lcd_h.display = pval
}

def on_meter_tick() {
    # Simulate a meter reading with try/catch
    raw = try_process(0.5)

    reading = MeterReading {
        rms: map_range(raw, -1.0, 1.0, -24.0, 0.0),
        peak: raw,
        clip_count: 0.0
    }

    info_h = flux_get_var("METER_INFO")
    info_h.text = format_reading(reading)

    bar_h = flux_get_var("METER_BAR")
    bar_h.value = clamp(map_range(raw, -1.0, 1.0, 0.0, 100.0), 0.0, 100.0)
}

def on_show_about() {
    flux_qt_msg_box("Modern FluxQt Dashboard",
        "Core features: enum  |  match  |  class  |  elif  |  bare return\n"
            + "generics [T]  |  impl  |  struct params  |  try/catch")
}

# ── Build UI ────────────────────────────────────────────────────────────────

def build_ui() {
    win = flux_qt_create_window("Audio Dashboard")

    # ── Signal section ──
    flux_qt_add_widget(win, flux_qt_create_label("=== Signal ==="))

    sig_combo = flux_qt_create_combobox()
    flux_qt_combo_add_item(sig_combo, "Sine 440 Hz")
    flux_qt_combo_add_item(sig_combo, "Square 220 Hz")
    flux_qt_combo_add_item(sig_combo, "White Noise")
    flux_qt_combo_add_item(sig_combo, "Sweep 20-20k")
    flux_qt_add_widget(win, sig_combo)

    sig_info = flux_qt_create_label("Select a signal")
    flux_qt_add_widget(win, sig_info)

    # ── Channel section ──
    flux_qt_add_widget(win, flux_qt_create_label("=== Channel ==="))

    gain_slider = flux_qt_create_slider(0.0)
    gain_slider.minimum = 0
    gain_slider.maximum = 100
    gain_slider.value = 50
    flux_qt_add_widget(win, gain_slider)

    gain_bar = flux_qt_create_progressbar()
    flux_qt_add_widget(win, gain_bar)

    gain_lcd = flux_qt_create_lcd()
    gain_lcd.digitCount = 4
    gain_lcd.display = 0.0
    flux_qt_add_widget(win, gain_lcd)

    pan_slider = flux_qt_create_slider(0.0)
    pan_slider.minimum = -100
    pan_slider.maximum = 100
    pan_slider.value = 0
    flux_qt_add_widget(win, pan_slider)

    pan_lcd = flux_qt_create_lcd()
    pan_lcd.digitCount = 4
    pan_lcd.display = 0.0
    flux_qt_add_widget(win, pan_lcd)

    # ── Meter section ──
    flux_qt_add_widget(win, flux_qt_create_label("=== Meter ==="))

    meter_info = flux_qt_create_label("Meter idle")
    flux_qt_add_widget(win, meter_info)

    meter_bar = flux_qt_create_progressbar()
    flux_qt_add_widget(win, meter_bar)

    # ── About ──
    about_btn = flux_qt_create_button("About")
    flux_qt_add_widget(win, about_btn)

    # Create class instances
    ch = ChannelStrip { gain: 1.0, pan: 0.0, mute: 0.0, solo: 0.0 }
    bus = MasterBus { channels: 2.0, level: 1.0 }

    # Wire signals (string-based binding)
    flux_qt_on_current_index_changed_by_name(sig_combo, "on_signal_changed")
    flux_qt_on_value_changed_by_name(gain_slider, "on_gain_changed")
    flux_qt_on_value_changed_by_name(pan_slider, "on_pan_changed")
    flux_qt_on_click_by_name(about_btn, "on_show_about")

    # Timer for meter updates
    flux_qt_create_timer(500.0, "on_meter_tick")
    flux_qt_timer_start(flux_get_var("timer"))

    # Store handles
    flux_set_var("SIG_COMBO", sig_combo)
    flux_set_var("SIG_INFO", sig_info)
    flux_set_var("GAIN_SLIDER", gain_slider)
    flux_set_var("GAIN_BAR", gain_bar)
    flux_set_var("GAIN_LCD", gain_lcd)
    flux_set_var("PAN_SLIDER", pan_slider)
    flux_set_var("PAN_LCD", pan_lcd)
    flux_set_var("METER_INFO", meter_info)
    flux_set_var("METER_BAR", meter_bar)
    flux_set_var("CHANNEL_STRIP", ch)
    flux_set_var("MASTER_BUS", bus)

    viora_flux_print("Audio dashboard loaded — enum, match, class, elif, generics, try/catch")
}

build_ui()
