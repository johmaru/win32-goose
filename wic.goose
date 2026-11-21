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
