// Saved states (save, load) and the reset of the state. Part of Game (game.h).
#include "game.h"

#include "game_internal.h"
#include "objects.h"

#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace rr {

namespace {

// Raw bytes in, zero runs packed: a zero byte is followed by the length of its run (1-255).
void pack(const uint8_t *data, size_t size, std::vector<uint8_t> &out) {
	for (size_t i = 0; i < size;) {
		if (data[i]) {
			out.push_back(data[i++]);
			continue;
		}
		size_t run = 0;
		while (i + run < size && data[i + run] == 0 && run < 255) run++;
		out.push_back(0);
		out.push_back(uint8_t(run));
		i += run;
	}
}

std::vector<uint8_t> unpack(const std::vector<uint8_t> &in) {
	std::vector<uint8_t> out;
	for (size_t i = 0; i < in.size(); i++) {
		if (in[i]) out.push_back(in[i]);
		else if (i + 1 < in.size()) out.insert(out.end(), in[++i], 0);
	}
	return out;
}

template <typename T> void put(std::vector<uint8_t> &raw, const T &value) {
	const auto *p = reinterpret_cast<const uint8_t *>(&value);
	raw.insert(raw.end(), p, p + sizeof value);
}

template <typename T> void get(const std::vector<uint8_t> &raw, size_t &at, T &value) {
	std::memcpy(&value, raw.data() + at, sizeof value);
	at += sizeof value;
}

}  // namespace

static_assert(std::is_trivially_copyable_v<World::State>, "saved as bytes");
// no padding, whose bytes would be whatever was in memory
static_assert(std::has_unique_object_representations_v<World::State>, "saved as bytes, no padding");

void Game::resetState() {
	Game fresh(rom_);
	std::vector<std::pair<void *, size_t>> to, from;
	eachState([&](auto &x) { to.push_back({&x, sizeof x}); });
	fresh.eachState([&](auto &x) { from.push_back({&x, sizeof x}); });
	for (size_t i = 0; i < to.size(); i++) std::memcpy(to[i].first, from[i].first, to[i].second);
}

std::vector<uint8_t> Game::save() const {
	std::vector<uint8_t> raw;
	eachState([&](auto &x) {
		using T = std::decay_t<decltype(x)>;
		static_assert(std::is_trivially_copyable_v<T> && (std::has_unique_object_representations_v<T> || std::is_same_v<T, bool>), "saved as bytes, no padding");
		put(raw, x);
	});
	put(raw, world_.state());
	put(raw, river_.lines.empty() ? World::State() : river_.lineStates.back());   // the river and the objects before the oldest line
	put(raw, uint16_t(river_.lines.size()));
	put(raw, river_.top);
	put(raw, resume_);
	put(raw, busy_);
	put(raw, gameOverText_);
	put(raw, pal_);
	std::vector<uint8_t> out;
	pack(raw.data(), raw.size(), out);
	return out;
}

void Game::load(const std::vector<uint8_t> &data) {
	const std::vector<uint8_t> raw = unpack(data);
	size_t at = 0;
	World::State now, oldest;
	uint16_t lines = 0;
	eachState([&](auto &x) { get(raw, at, x); });
	get(raw, at, now);
	world_.setState(now);
	get(raw, at, oldest);
	get(raw, at, lines);
	get(raw, at, river_.top);
	get(raw, at, resume_);
	get(raw, at, busy_);
	get(raw, at, gameOverText_);
	get(raw, at, pal_);
	river_.rebuild(world_, oldest, lines);   // screen memory again, from the generator's state before its oldest line
	scan_ = {};
}

}  // namespace rr
