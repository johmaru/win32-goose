// Pure Goose UTF-8 <-> UTF-16LE conversion helpers for Win32 *W APIs.
namespace win32;

// Appends UTF-16 code units of `s` followed by a NUL terminator to `out`.
fn utf8_to_utf16z(s: const u8[:], out: u16[>..]&) {
    var i: i64 = 0;
    while i < s.len {
        let b0 = s[i] as u32;
        if b0 < 0x80 {
            out.push(b0 as! u16);
            i += 1;
        } else if (b0 & 0xE0) == 0xC0 && i + 1 < s.len {
            let b1 = s[i + 1] as u32;
            let cp = ((b0 & 0x1F) << 6) | (b1 & 0x3F);
            out.push(cp as! u16);
            i += 2;
        } else if (b0 & 0xF0) == 0xE0 && i + 2 < s.len {
            let b1 = s[i + 1] as u32;
            let b2 = s[i + 2] as u32;
            let cp = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
            out.push(cp as! u16);
            i += 3;
        } else if (b0 & 0xF8) == 0xF0 && i + 3 < s.len {
            let b1 = s[i + 1] as u32;
            let b2 = s[i + 2] as u32;
            let b3 = s[i + 3] as u32;
            let cp = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
            let adj = cp - 0x10000;
            out.push((0xD800 + (adj >> 10)) as! u16);
            out.push((0xDC00 + (adj & 0x3FF)) as! u16);
            i += 4;
        } else {
            // Replacement character U+FFFD on invalid byte sequence.
            out.push(0xFFFD);
            i += 1;
        }
    }
    out.push(0);
}

fn utf8_to_utf16z(s: const u8[:]) -> u16[>..] {
    var out: u16[>..] = [];
    utf8_to_utf16z(s, out);
    return out;
}

// Decodes UTF-16 code units (stopping at the first NUL if present) into UTF-8.
fn utf16_to_utf8(ws: const u16[:], out: u8[>..]&) {
    var i: i64 = 0;
    while i < ws.len {
        let u0 = ws[i] as u32;
        if u0 == 0 { break; }
        var cp: u32 = u0;
        if u0 >= 0xD800 && u0 <= 0xDBFF && i + 1 < ws.len {
            let u1 = ws[i + 1] as u32;
            if u1 >= 0xDC00 && u1 <= 0xDFFF {
                cp = 0x10000 + (((u0 - 0xD800) << 10) | (u1 - 0xDC00));
                i += 2;
            } else {
                cp = 0xFFFD;
                i += 1;
            }
        } else {
            i += 1;
        }

        if cp < 0x80 {
            out.push(cp as! u8);
        } else if cp < 0x800 {
            out.push((0xC0 | (cp >> 6)) as! u8);
            out.push((0x80 | (cp & 0x3F)) as! u8);
        } else if cp < 0x10000 {
            out.push((0xE0 | (cp >> 12)) as! u8);
            out.push((0x80 | ((cp >> 6) & 0x3F)) as! u8);
            out.push((0x80 | (cp & 0x3F)) as! u8);
        } else {
            out.push((0xF0 | (cp >> 18)) as! u8);
            out.push((0x80 | ((cp >> 12) & 0x3F)) as! u8);
            out.push((0x80 | ((cp >> 6) & 0x3F)) as! u8);
            out.push((0x80 | (cp & 0x3F)) as! u8);
        }
    }
}

fn utf16_to_utf8(ws: const u16[:]) -> u8[>..] {
    var out: u8[>..] = [];
    utf16_to_utf8(ws, out);
    return out;
}
