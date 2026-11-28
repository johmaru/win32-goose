// Windows Imaging Component (WIC): PNG, JPEG, BMP image decoding and encoding.
namespace win32;

// Decodes an image file (PNG, JPEG, BMP, GIF, TIFF) into top-down 32bpp BGRA
// pixels (0xAARRGGBB, directly compatible with dib::present_pixels / blit_pixels).
extern fn image_load_bgra(
    path: const u8[:],
    out_pixels: u32[>..]&,
    out_width: i32&,
    out_height: i32&
) -> bool = "w32_image_load_bgra";

// Decodes an image file into top-down 32bpp RGBA pixels (0xAABBGGRR, directly
// compatible with D3D11 DXGI_FORMAT_R8G8B8A8_UNORM and OpenGL GL_RGBA).
extern fn image_load_rgba(
    path: const u8[:],
    out_pixels: u32[>..]&,
    out_width: i32&,
    out_height: i32&
) -> bool = "w32_image_load_rgba";

// Decodes an image from in-memory bytes (PNG, ICO, JPEG, BMP, GIF, WebP, TIFF)
// into top-down 32bpp BGRA pixels (selecting the largest frame <= 256px for ICO).
extern fn image_load_bgra_mem(
    data: const u8[:],
    out_pixels: u32[>..]&,
    out_width: i32&,
    out_height: i32&
) -> bool = "w32_image_load_bgra_mem";

// Decodes an image from in-memory bytes into top-down 32bpp RGBA pixels.
extern fn image_load_rgba_mem(
    data: const u8[:],
    out_pixels: u32[>..]&,
    out_width: i32&,
    out_height: i32&
) -> bool = "w32_image_load_rgba_mem";

// Resizes top-down 32bpp BGRA pixels using WIC Fant interpolation.
extern fn image_resize_bgra(
    src_pixels: const u32[:],
    src_w: i32,
    src_h: i32,
    dst_w: i32,
    dst_h: i32,
    out_pixels: u32[>..]&
) -> bool = "w32_image_resize_bgra";

// Extracts an icon from a Windows executable (.exe), shortcut (.lnk), .ico, or
// PATH-resolved executable name (e.g. "notepad.exe") into top-down 32bpp BGRA.
extern fn extract_file_icon_bgra(
    path: const u8[:],
    desired_size: i32,
    out_pixels: u32[>..]&,
    out_width: i32&,
    out_height: i32&
) -> bool = "w32_extract_file_icon_bgra";

// Encodes top-down 32bpp BGRA pixels (0xAARRGGBB) to a PNG file.
extern fn image_save_png(
    path: const u8[:],
    pixels_bgra: const u32[:],
    width: i32,
    height: i32
) -> bool = "w32_image_save_png";

// Encodes top-down 32bpp BGRA pixels (0xAARRGGBB) to a BMP file.
extern fn image_save_bmp(
    path: const u8[:],
    pixels_bgra: const u32[:],
    width: i32,
    height: i32
) -> bool = "w32_image_save_bmp";
