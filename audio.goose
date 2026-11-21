// Win32 Audio API: WASAPI output device enumeration, low-latency shared-mode PCM
// render stream, and winmm PlaySound helpers.
namespace win32;

import types;

extern fn audio_output_device_count() -> i32 = "w32_audio_output_device_count";
extern fn audio_output_device_name(index: i32, out: u8[>..]&) -> bool = "w32_audio_output_device_name";
extern fn audio_default_output_name(out: u8[>..]&) -> bool = "w32_audio_default_output_name";

extern fn shim_wasapi_open_default(out_fmt: AudioFormat&) -> u32 = "w32_wasapi_open_default";
extern fn shim_wasapi_available_frames(audio_id: u32) -> u32 = "w32_wasapi_available_frames";
extern fn shim_wasapi_write_f32(audio_id: u32, samples: const f32[:]) -> u32 = "w32_wasapi_write_f32";
extern fn shim_wasapi_start(audio_id: u32) = "w32_wasapi_start";
extern fn shim_wasapi_stop(audio_id: u32) = "w32_wasapi_stop";
extern fn shim_wasapi_close(audio_id: u32) = "w32_wasapi_close";

extern fn play_sound_file(path: const u8[:], async_play: bool = true) -> bool = "w32_play_sound_file";
extern fn play_sound_stop() = "w32_play_sound_stop";
extern fn play_wav_bytes(wav_bytes: const u8[:], async_play: bool = false) -> bool = "w32_play_wav_bytes";

fn audio_default_output_name() -> u8[>..] {
    var out: u8[>..] = [];
    audio_default_output_name(out);
    return out;
}

fn wasapi_open_default(out_fmt: AudioFormat&) -> Haudio {
    return Haudio { id: shim_wasapi_open_default(out_fmt) };
}

fn available_frames(a: Haudio) -> u32 { shim_wasapi_available_frames(a.id) }
fn write_f32(a: Haudio, samples: const f32[:]) -> u32 { shim_wasapi_write_f32(a.id, samples) }
fn start(a: Haudio) { shim_wasapi_start(a.id); }
fn stop(a: Haudio) { shim_wasapi_stop(a.id); }
fn close(a: Haudio) { shim_wasapi_close(a.id); }

// --- WASAPI Audio Capture (Microphone & System Loopback) --------------------

extern fn audio_input_device_count() -> i32 = "w32_audio_input_device_count";
extern fn audio_input_device_name(index: i32, out: u8[>..]&) -> bool = "w32_audio_input_device_name";
extern fn audio_default_input_name(out: u8[>..]&) -> bool = "w32_audio_default_input_name";

extern fn shim_wasapi_capture_open(loopback: bool, out_fmt: AudioFormat&) -> u32 = "w32_wasapi_capture_open";
extern fn shim_wasapi_capture_read_f32(cap_id: u32, out_f32: f32[>..]&) -> u32 = "w32_wasapi_capture_read_f32";
extern fn shim_wasapi_capture_start(cap_id: u32) = "w32_wasapi_capture_start";
extern fn shim_wasapi_capture_stop(cap_id: u32) = "w32_wasapi_capture_stop";
extern fn shim_wasapi_capture_close(cap_id: u32) = "w32_wasapi_capture_close";

fn audio_default_input_name() -> u8[>..] {
    var out: u8[>..] = [];
    audio_default_input_name(out);
    return out;
}

// Opens default WASAPI microphone (loopback = false) or desktop audio loopback
// (loopback = true) in shared mode.
fn wasapi_capture_open(out_fmt: AudioFormat&, loopback: bool = false) -> HaudioIn {
    return HaudioIn { id: shim_wasapi_capture_open(loopback, out_fmt) };
}

fn read_f32(c: HaudioIn, out_samples: f32[>..]&) -> u32 { shim_wasapi_capture_read_f32(c.id, out_samples) }
fn start(c: HaudioIn) { shim_wasapi_capture_start(c.id); }
fn stop(c: HaudioIn) { shim_wasapi_capture_stop(c.id); }
fn close(c: HaudioIn) { shim_wasapi_capture_close(c.id); }
