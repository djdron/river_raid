// See display_mode.h. C++/CX (compiled with /ZW, see uwp.cmake); called on the app's thread
// (SDL's WinRT entry point runs the game on the CoreWindow's thread), so nothing waits on the
// asynchronous requests.
#include "display_mode.h"

#include <cmath>

using namespace Windows::Graphics::Display::Core;

namespace uwp {
namespace {

bool changed = false;

HdmiDisplayInformation ^hdmi() {
	try {
		return HdmiDisplayInformation::GetForCurrentView();
	} catch (Platform::Exception ^) {
		return nullptr;
	}
}

}  // namespace

double displayRefreshRate() {
	HdmiDisplayInformation ^info = hdmi();
	if (!info) return 0;
	try {
		HdmiDisplayMode ^mode = info->GetCurrentDisplayMode();
		return mode ? mode->RefreshRate : 0;
	} catch (Platform::Exception ^) {
		return 0;
	}
}

bool requestRefreshRate(double hz) {
	HdmiDisplayInformation ^info = hdmi();
	if (!info) return false;
	try {
		HdmiDisplayMode ^current = info->GetCurrentDisplayMode();
		if (current && std::abs(current->RefreshRate - hz) < 0.5) return true;   // already
		auto modes = info->GetSupportedDisplayModes();
		// the same resolution at hz; best also with the same colour space and depth
		HdmiDisplayMode ^best = nullptr;
		int bestScore = -1;
		for (unsigned i = 0; i < modes->Size; i++) {
			HdmiDisplayMode ^m = modes->GetAt(i);
			if (std::abs(m->RefreshRate - hz) >= 0.5 || m->StereoEnabled) continue;
			int score = 0;
			if (current) {
				if (m->ResolutionWidthInRawPixels != current->ResolutionWidthInRawPixels ||
						m->ResolutionHeightInRawPixels != current->ResolutionHeightInRawPixels) continue;
				if (m->ColorSpace == current->ColorSpace) score += 2;
				if (m->BitsPerPixel == current->BitsPerPixel) score += 1;
			}
			if (score > bestScore) {
				best = m;
				bestScore = score;
			}
		}
		if (!best) return false;
		info->RequestSetCurrentDisplayModeAsync(best);   // the TV switches in a moment
		changed = true;
		return true;
	} catch (Platform::Exception ^) {
		return false;
	}
}

void restoreDisplayMode() {
	if (!changed) return;
	changed = false;
	HdmiDisplayInformation ^info = hdmi();
	if (!info) return;
	try {
		info->SetDefaultDisplayModeAsync();
	} catch (Platform::Exception ^) {
	}
}

}  // namespace uwp
