// The Atari OS character set.
//
// The game copies most of its letters and digits from the OS ROM font at power-on (A0FB-A113)
// and replaces the rest with its own characters from the cartridge. The OS ROM is not part of
// the cartridge, so the port has two sources for those characters:
//   - a file with the OS ROM (or just its 1 KB font) that the user has, for the exact look;
//   - a built-in font drawn for this port (letters and digits only), used otherwise.
#pragma once
#include <cstdint>
#include <string>

namespace rr {

constexpr int kOsFontBytes = 1024;   // 128 characters x 8 rows, in ATASCII internal order

// Reads the font from an OS ROM image (10 KB OS-B at $D800, 16 KB XL/XE at $C000) or from a
// 1 KB font file. Returns false if the file has none of these sizes.
bool loadOsFont(const std::string &path, uint8_t font[kOsFontBytes]);

// Built-in replacement: digits, upper and lower case letters; other characters are blank.
void builtinOsFont(uint8_t font[kOsFontBytes]);

}  // namespace rr
