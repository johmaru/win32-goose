// Win32 Video API: Media Foundation IMFSourceReader decoding directly into
// RGB32 u32[:] pixel buffers (compatible with dib.goose StretchDIBits).
namespace win32;

import types;

extern fn mf_startup() -> bool = "w32_mf_startup";
extern fn mf_shutdown() = "w32_mf_shutdown";
extern fn shim_video_open(path: const u8[:], out_info: VideoInfo&) -> u32 = "w32_video_open";
extern fn shim_video_read_frame_rgb32(vid_id: u32, out_pixels: u32[:], out_ts_100ns: i64&) -> bool = "w32_video_read_frame_rgb32";
extern fn shim_video_seek_100ns(vid_id: u32, pos_100ns: i64) -> bool = "w32_video_seek_100ns";
extern fn shim_video_close(vid_id: u32) = "w32_video_close";

fn video_open(path: const u8[:], out_info: VideoInfo&) -> Hvideo {
    return Hvideo { id: shim_video_open(path, out_info) };
}

fn read_frame_rgb32(v: Hvideo, out_pixels: u32[:], out_ts_100ns: i64&) -> bool {
    return v.is_valid() && shim_video_read_frame_rgb32(v.id, out_pixels, out_ts_100ns);
}

fn seek_100ns(v: Hvideo, pos_100ns: i64) -> bool {
    return v.is_valid() && shim_video_seek_100ns(v.id, pos_100ns);
}

fn close(v: Hvideo) {
    shim_video_close(v.id);
}
