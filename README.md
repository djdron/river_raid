# River Raid (Atari 8-bit) → C++

English | [Русский](README.ru.md)

Reverse engineering and C++ port of Activision's River Raid for Atari 400/800 (1983),
aiming for 1:1 game logic and 1:1 sound on modern Windows, Linux and macOS.

- `HANDOFF.md` — project state, verified facts, decisions, next steps (start here)
- `docs/NOTES.md` — findings with addresses
- `disasm/` — annotated disassembly
- `tools/` — disassembler, verification scripts
- `oracle/` — reference runner on libatari800 (frame-by-frame RAM / POKEY traces)
- `rom/` — put your own ROM dump here (not included)
