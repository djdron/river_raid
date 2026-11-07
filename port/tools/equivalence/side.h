#pragma once
#include <cstdint>
struct SideInput { uint8_t stick[2]; bool trigger[2], start, select, option, key; };
struct Snapshot {
	uint8_t ram[0x4000];
	uint8_t screen[256][48];
	int lines;
	uint8_t picture[240][384];
	uint8_t audf[4], audc[4];
	bool busy;
	uint8_t colours[5];
};
#define DECL(P) \
	void P##Init(const char *, int, const uint8_t *, int, bool); \
	int P##Frame(const SideInput &, uint32_t &); \
	void P##Snapshot(Snapshot &); \
	void P##GenLine(uint8_t line[48], uint8_t *ram);
DECL(old)
DECL(new)
