// Where the port keeps each byte of the original's RAM ($0000-$3FFF), for the checks against
// traces of the original: loading a state from a RAM image, comparing with one, writing one.
// Every class lists its own bytes (its mapRam()); World and Game put theirs together.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace rr {

class RamMap {
public:
	enum Flags : uint8_t {
		kGameOwned = 1,   // the river's or objects' byte that the rest of the game also changes:
		                  // World::loadGameOwned takes it from the RAM, the generator's check skips it
		kSlot = 2,        // an object slot's: compared only while the slot is in use (index >= $4D)
	};

	struct Byte {
		const char *name;
		int index;          // in an array, or -1
		uint16_t addr;
		uint8_t flags;
		uint8_t *value;     // the byte, or
		uint16_t *word;     // a byte of this word
		int shift;          // (0 low, 8 high)

		uint8_t get() const { return value ? *value : uint8_t(*word >> shift); }
		void set(uint8_t v) const {
			if (value) *value = v;
			else *word = uint16_t((*word & ~(0xFF << shift)) | v << shift);
		}
	};

	// One byte; `index`: its place in an array of the same name (slots)
	RamMap &byte(const char *name, uint16_t addr, uint8_t &v, uint8_t flags = 0, int index = -1) {
		bytes_.push_back({name, index, addr, flags, &v, nullptr, 0});
		return *this;
	}
	// n bytes from addr up
	RamMap &bytes(const char *name, uint16_t addr, uint8_t *v, int n, uint8_t flags = 0) {
		for (int i = 0; i < n; i++) bytes_.push_back({name, i, uint16_t(addr + i), flags, v + i, nullptr, 0});
		return *this;
	}
	// A 16-bit value, low byte first
	RamMap &word(const char *name, uint16_t lo, uint16_t hi, uint16_t &v, uint8_t flags = 0) {
		bytes_.push_back({name, 0, lo, flags, nullptr, &v, 0});
		bytes_.push_back({name, 1, hi, flags, nullptr, &v, 8});
		return *this;
	}

	const std::vector<Byte> &list() const { return bytes_; }

	// From a RAM image (only the game-owned bytes if `gameOwnedOnly`)
	void load(const uint8_t *ram, bool gameOwnedOnly = false) const;
	void save(uint8_t *ram) const;
	// "name $addr: port=.. orig=.." (name[i] $addr in arrays) for each byte that differs
	void diff(const uint8_t *ram, bool withGameOwned, std::vector<std::string> &out) const;

private:
	std::vector<Byte> bytes_;
};

}  // namespace rr
