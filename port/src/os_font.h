// The Atari OS character set.
//
// The game copies most of its letters and digits from the OS ROM font at power-on (A0FB-A113)
// and replaces the rest with its own characters from the cartridge. The OS ROM is not part of
// the cartridge. The port has the font of AltirraOS built in: the free replacement OS that
// ships with the atari800 emulator (and that the reference traces in oracle/ were made with).
// The font of Atari's own OS ROM can be loaded from a file instead.
#pragma once
#include <cstdint>
#include <string>

namespace rr {

constexpr int kOsFontBytes = 1024;   // 128 characters x 8 rows, in ATASCII internal order

// Reads the font from an OS ROM image (10 KB OS-B at $D800, 16 KB XL/XE at $C000) or from a
// 1 KB font file. Returns false if the file has none of these sizes.
bool loadOsFont(const std::string &path, uint8_t font[kOsFontBytes]);

// Built-in font: AltirraOS (permissive licence, see os_font.cpp).
void builtinOsFont(uint8_t font[kOsFontBytes]);

}  // namespace rr
