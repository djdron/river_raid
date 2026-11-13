// What UWP gives a game beyond SDL: the TV's display mode on the Xbox (50 Hz for the PAL
// machine), through Windows.Graphics.Display.Core.HdmiDisplayInformation, which on a PC gives
// nothing (the functions return 0 / false); and the arguments the app was started with.
#pragma once

#include <string>
#include <vector>

namespace uwp {

// The command line the app was started with: from a link riverraid:ARGS (the protocol the
// manifest declares; Dev Home's "Launch with parameters" with "Use custom protocol"), ARGS
// with %XX decoded, or the arguments of a plain launch; split at spaces.
std::vector<std::string> launchArguments();

// Refresh rate of the TV's current mode, 0 if unknown.
double displayRefreshRate();
// Asks the TV for a mode of the same resolution at about hz (the console has to allow it: on
// the Xbox, Settings - TV & display options - Video modes - Allow 50 Hz). true if such a mode
// was found and asked for; the switch takes a moment.
bool requestRefreshRate(double hz);
// Back to the console's own mode, if requestRefreshRate() changed it.
void restoreDisplayMode();

}  // namespace uwp
