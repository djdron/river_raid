// The river on the screen. See river.h.
#include "river.h"

namespace rr {

River::Runs River::makeRuns(const uint8_t *line) {
	Runs r;
	int prev = 0;
	for (int p = 0; p < kLineBytes * 4; p++) {
		const int v = (line[p >> 2] >> (6 - 2 * (p & 3))) & 3;
		if (v == prev) continue;
		if (prev) r.b[r.n++] = uint8_t(p);
		if (v) { r.a[r.n] = uint8_t(p); r.colour[r.n] = uint8_t(v); }
		prev = v;
	}
	if (prev) r.b[r.n++] = uint8_t(kLineBytes * 4);
	return r;
}

void River::clear() {
	lines.clear();
	bankLines.clear();
	runs.clear();
	lineStates.clear();
}

void River::add(const Line &line, const Line &bank) {
	lines.insert(lines.begin(), line);
	bankLines.insert(bankLines.begin(), bank);
	runs.insert(runs.begin(), makeRuns(bank.data()));
}

void River::push(World &world) {
	Line line, bank;
	if (keepLineStates) {
		lineStates.insert(lineStates.begin(), world.state());
		if (lineStates.size() > 256) lineStates.pop_back();
	}
	world.nextLine(line.data(), bank.data());
	add(line, bank);
	if (lines.size() > 256) {
		lines.pop_back();
		bankLines.pop_back();
		runs.pop_back();
	}
	uint8_t topLo = uint8_t(top), topHi = uint8_t(top >> 8);
	if (topLo < kLineBytes) topHi--;
	topLo = uint8_t(topLo - kLineBytes);
	if (topHi < 0x20) topHi = 0x3E;
	if (topHi == 0x2F) topHi = 0x2E;
	top = uint16_t(topLo | (topHi << 8));
}

void River::rebuild(World world, const World::State &oldest, int count) {
	world.setState(oldest);
	clear();
	Line line, bank;
	for (int i = 0; i < count; i++) {
		if (keepLineStates) lineStates.insert(lineStates.begin(), world.state());
		world.nextLine(line.data(), bank.data());
		add(line, bank);
	}
}

}  // namespace rr
