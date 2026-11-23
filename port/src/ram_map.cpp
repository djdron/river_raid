#include "ram_map.h"

#include <cstdio>

namespace rr {

namespace {
constexpr uint16_t kFirstSlot = 0x4D;   // the first object slot in use (Objects::firstSlot)
}

void RamMap::load(const uint8_t *ram, bool gameOwnedOnly) const {
	for (const Byte &b : bytes_)
		if (!gameOwnedOnly || (b.flags & kGameOwned)) b.set(ram[b.addr]);
}

void RamMap::save(uint8_t *ram) const {
	for (const Byte &b : bytes_) ram[b.addr] = b.get();
}

void RamMap::diff(const uint8_t *ram, bool withGameOwned, std::vector<std::string> &out) const {
	char text[160];
	for (const Byte &b : bytes_) {
		if ((b.flags & kGameOwned) && !withGameOwned) continue;
		if ((b.flags & kSlot) && b.index < ram[kFirstSlot]) continue;   // free: rewritten before it is read
		const uint8_t v = b.get();
		if (v == ram[b.addr]) continue;
		if (b.index < 0) std::snprintf(text, sizeof text, "%s $%02X: port=%02X orig=%02X", b.name, b.addr, v, ram[b.addr]);
		else std::snprintf(text, sizeof text, "%s[%d] $%04X: port=%02X orig=%02X", b.name, b.index, b.addr, v, ram[b.addr]);
		out.push_back(text);
	}
}

}  // namespace rr
