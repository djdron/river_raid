// The river on the screen. See river.h.
#include "river.h"

#include <cstring>

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
	lines.push(line);
	bankLines.push(bank);
	runs.push(makeRuns(bank.data()));
}

void River::push(World &world) {
	Line line, bank;
	if (keepLineStates) lineStates.push(world.state());
	world.nextLine(line.data(), bank.data());
	add(line, bank);
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
		if (keepLineStates) lineStates.push(world.state());
		world.nextLine(line.data(), bank.data());
		add(line, bank);
	}
}

namespace {

std::vector<River::Line> screenFromRam(const uint8_t *ram) {
	std::vector<River::Line> lines;
	int pc = 0x3F03;   // after 70 F0 30
	uint16_t addr = 0;
	while (lines.size() < 160 && pc < 0x3FFF) {
		const uint8_t op = ram[pc++];
		if ((op & 0x0F) != 0x0E) break;
		if (op & 0x40) {   // LMS: a new address
			addr = uint16_t(ram[pc] | (ram[pc + 1] << 8));
			pc += 2;
		}
		River::Line l;
		std::memcpy(l.data(), ram + addr, kLineBytes);
		lines.push_back(l);
		addr = uint16_t(addr + kLineBytes);
	}
	return lines;
}

}  // namespace

void River::linesFromRam(const uint8_t *ram) {
	const auto shown = screenFromRam(ram);   // top first
	lines.clear();
	for (size_t i = shown.size(); i-- > 0;) lines.push(shown[i]);
}

int River::lineDifferingFromRam(const uint8_t *ram) const {
	const auto shown = screenFromRam(ram);
	for (size_t i = 0; i < shown.size() && i < lines.size(); i++)
		if (shown[i] != lines[i]) return int(i);
	return -1;
}

}  // namespace rr
