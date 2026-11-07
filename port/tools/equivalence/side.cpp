// One side of the refactoring equivalence check, compiled twice: once against the old sources
// (namespace renamed to rr_old) and once against the new ones.
#include <cstring>
#include "game.h"
#include "side.h"

#define CAT2(a, b) a##b
#define CAT(a, b) CAT2(a, b)
#define F(name) CAT(SIDE, name)

namespace {
rr::Rom *rom;
rr::Game *game;
rr::RiverGen *gen;
}

void F(Init)(const char *romPath, int mode, const uint8_t *ram, int gameNumber, bool pal) {
	rom = new rr::Rom;
	rom->load(romPath);
	game = new rr::Game(*rom);
	gen = new rr::RiverGen(*rom);
	if (mode == 0) game->startAttract(pal);
	else if (mode == 1) game->loadFromRam(ram);
	else gen->startGame(gameNumber, pal);
}

int F(Frame)(const SideInput &in, uint32_t &rng) {
	rr::Input i;
	i.stick[0] = in.stick[0]; i.stick[1] = in.stick[1];
	i.trigger[0] = in.trigger[0]; i.trigger[1] = in.trigger[1];
	i.start = in.start; i.select = in.select; i.option = in.option; i.key = in.key;
	int draws = 0;
	game->frame(i, [&]() { draws++; rng = rng * 1664525u + 1013904223u; return uint8_t(rng >> 24); });
	return draws;
}

void F(Snapshot)(Snapshot &out) {
	std::memset(out.ram, 0, sizeof out.ram);
	game->saveToRam(out.ram);
	out.lines = int(game->screen.size());
	for (int k = 0; k < out.lines && k < 256; k++) std::memcpy(out.screen[k], game->screen[size_t(k)].data(), 48);
	game->drawRiver(out.picture);
	for (int k = 0; k < 4; k++) { out.audf[k] = game->g.pokey.audf[k]; out.audc[k] = game->g.pokey.audc[k]; }
	out.busy = game->busy();
	out.colours[0] = game->g.colourBank; out.colours[1] = game->g.colourEdge; out.colours[2] = game->g.colourWater;
	out.colours[3] = game->g.logoShown; out.colours[4] = game->g.shellColour;
}

void F(GenLine)(uint8_t line[48], uint8_t *ram) {
	gen->nextLine(line);
	std::memset(ram, 0, 0x4000);
	gen->saveToRam(ram);
}
