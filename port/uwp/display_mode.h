// The TV's display mode on the Xbox (UWP): switching it to 50 Hz for the PAL machine. Through
// Windows.Graphics.Display.Core.HdmiDisplayInformation, which SDL does not use; on a PC it
// gives nothing (no HDMI information), and the functions return 0 / false.
#pragma once

namespace uwp {

// Refresh rate of the TV's current mode, 0 if unknown.
double displayRefreshRate();
// Asks the TV for a mode of the same resolution at about hz (the console has to allow it: on
// the Xbox, Settings - TV & display options - Video modes - Allow 50 Hz). true if such a mode
// was found and asked for; the switch takes a moment.
bool requestRefreshRate(double hz);
// Back to the console's own mode, if requestRefreshRate() changed it.
void restoreDisplayMode();

}  // namespace uwp
