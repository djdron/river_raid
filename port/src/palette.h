// Colours of the Atari: RGB for the 256 colour codes the game writes into the colour
// registers (hue << 4 | luminance), as an NTSC or a PAL machine shows them.
#pragma once

#include <cstdint>

namespace rr {

// Two models of the GTIA's video output, each with its default settings:
//   Altirra:  the emulator Altirra (luminance levels measured on an 800XL, hues from its
//             default NTSC / PAL settings, NTSC primaries converted to sRGB); black is 0 and
//             white 255. The default.
//   Atari800: the emulator atari800, the reference the port is checked against (luminance
//             levels from GTIA data, hues from the colour delay line, CRT gamma 2.35 converted
//             to sRGB); darker in the middle tones. Stretched to black 0 / white 255 (atari800
//             itself uses the video levels 16-235).
enum class PaletteModel { Altirra, Atari800 };

const char *paletteName(PaletteModel model);

// sRGB colour of an Atari colour code.
void atariRgb(uint8_t colour, uint8_t rgb[3], bool pal = false, PaletteModel model = PaletteModel::Altirra);

}  // namespace rr
