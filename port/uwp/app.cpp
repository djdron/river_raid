// See app.h. C++/CX (compiled with /ZW, see uwp.cmake); called on the app's thread
// (SDL's WinRT entry point runs the game on the CoreWindow's thread), so nothing waits on the
// asynchronous requests.
#include "app.h"

#include <cmath>
#include <cctype>
#include <cstdlib>

using namespace Windows::ApplicationModel::Activation;
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

std::string utf8(Platform::String ^s) {
	std::string out;
	if (!s) return out;
	for (const wchar_t *p = s->Data(); *p; p++) {
		const unsigned c = unsigned(*p);
		if (c < 0x80) {
			out += char(c);
		} else if (c < 0x800) {
			out += char(0xC0 | c >> 6);
			out += char(0x80 | (c & 0x3F));
		} else {   // (no surrogate pairs in options)
			out += char(0xE0 | c >> 12);
			out += char(0x80 | (c >> 6 & 0x3F));
			out += char(0x80 | (c & 0x3F));
		}
	}
	return out;
}

std::string percentDecoded(const std::string &s) {
	std::string out;
	for (size_t i = 0; i < s.size(); i++) {
		if (s[i] == '%' && i + 2 < s.size() && std::isxdigit((unsigned char)s[i + 1]) && std::isxdigit((unsigned char)s[i + 2])) {
			out += char(std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16));
			i += 2;
		} else {
			out += s[i];
		}
	}
	return out;
}

}  // namespace

std::vector<std::string> launchArguments() {
	std::string line;
	try {
		IActivatedEventArgs ^args = Windows::ApplicationModel::AppInstance::GetActivatedEventArgs();
		if (args && args->Kind == ActivationKind::Protocol) {
			const std::string uri = utf8(static_cast<ProtocolActivatedEventArgs ^>(args)->Uri->AbsoluteUri);
			const size_t colon = uri.find(':');
			line = percentDecoded(colon == std::string::npos ? std::string() : uri.substr(colon + 1));
			while (!line.empty() && line[0] == '/') line.erase(0, 1);   // riverraid://-pal
			while (!line.empty() && line.back() == '/') line.pop_back();
		} else if (args && args->Kind == ActivationKind::Launch) {
			line = utf8(static_cast<LaunchActivatedEventArgs ^>(args)->Arguments);
		}
	} catch (Platform::Exception ^) {
	}
	std::vector<std::string> words;
	std::string word;
	for (char c : line + " ") {
		if (c == ' ' || c == '+') {   // (a space can come as + in a link)
			if (!word.empty()) words.push_back(word);
			word.clear();
		} else {
			word += c;
		}
	}
	return words;
}

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
