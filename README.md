# River Raid (Atari 8-bit) → C++

English | [Русский](README.ru.md)

Reverse engineering and C++ port of Activision's River Raid for Atari 400/800 (1983),
aiming for 1:1 game logic and 1:1 sound on modern Windows, Linux and macOS.

- `HANDOFF.md` — project state, verified facts, decisions, next steps (start here)
- `docs/HOW_IT_WORKS.md` — how the game works, for everyone: the river, enemies, bridges, collisions
- `docs/NOTES.md` — findings with addresses
- `disasm/` — annotated disassembly
- `tools/` — disassembler, verification scripts
- `oracle/` — reference runner on libatari800 (frame-by-frame RAM / POKEY traces)
- `port/` — C++ port: the game (`river_raid`), the river viewer, check tools
- `rom/` — put your own ROM dump here (not included)

## Disclaimer
This is an unofficial fan project, made for educational purposes: to study how a 1983 game works, how its code makes the river, the enemies and the sound, and to show it in readable C++. It is not affiliated with, endorsed or sponsored by Activision or Microsoft. River Raid and Activision are trademarks of their owners; they are used here only to say which game this is about.

No part of the game is included: no ROM, no graphics, no sound. To run the port you need your own dump of the River Raid cartridge (8 KB, see `rom/README.md`). Please do not open issues asking where to get one.
