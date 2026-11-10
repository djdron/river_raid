// River Raid game frame: what the original does once per TV frame, around the river generator.
//
// The original's main loop (A197-A6EA, AE99-AF52) and its interrupts run in this order in each
// frame, and Game::frame() follows it:
//   1. prepare(): at the top of the frame (AE99) copy the tank, tank shell, helicopter missile
//      and plane shapes into player-missile memory;
//   2. display(): the screen is drawn; the display list interrupt (DLI) re-uses player 3 for
//      every object slot down the river and reads the hardware collision registers after each
//      object (display.cpp);
//   3. logic(): from scanline 160 (A197): object activation and movement, the tank on the
//      road and its shell, the helicopter missile, then 1 river line per frame (A6FD-A711 and
//      the generator);
//   4. vbi(): the vertical blank interrupt (B2A5): sound driver, frame counter, timers.
//
// Everything the original does is ported: the attract mode (no player, $76 = $FF, $29 bit 7),
// starting a game (START), choosing the game (SELECT / OPTION), the plane, its speed, firing,
// fuel, collisions, explosions, score and lives, two-player games, death, respawn, game over,
// the high score of each game, pause (any key).
//
// Field comments give the original RAM address, for comparing with traces (tools/game_check).
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "river_draw.h"
#include "river_gen.h"
#include "status_bar.h"

namespace rr {

// Player-missile memory $0B00-$0FFF (PMBASE $08, single-line resolution: one byte per
// scanline). $0B00 missiles (bits 1-0 missile 0, 3-2 missile 1, 5-4 missile 2), then players
// 0-3 at $0C00, $0D00, $0E00, $0F00. Kept as one block: the original's copy loops can run
// past the end of one page into the next.
struct PmMemory {
	static constexpr uint16_t kBase = 0x0B00;
	std::array<uint8_t, 0x500> bytes{};
	uint8_t &at(uint16_t addr) { return bytes[size_t(addr - kBase) % bytes.size()]; }
	uint8_t missiles(int line) const { return bytes[size_t(line & 0xFF)]; }
	uint8_t player(int n, int line) const { return bytes[size_t(0x100 * (n + 1) + (line & 0xFF))]; }
};

// POKEY sound registers ($D200-$D207): frequency divider and control (distortion, volume)
// of the four channels. AUDCTL ($D208) stays 0 in River Raid.
struct PokeyRegs {
	uint8_t audf[4] = {}, audc[4] = {};
};

// Collision results of one frame, set by the DLI to the slot whose band saw the collision
// ($FF = none). Addresses $1A-$22.
struct Collisions {
	uint8_t bulletHitBank = 0xFF;    // $1A missile 2 (the plane's bullet) with bank or edge
	uint8_t planeHitBank = 0xFF;     // $1B player 2 (the plane) with the bank
	uint8_t bulletHitObject = 0xFF;  // $1C the bullet with an object (player 3)
	uint8_t planeHitObject = 0xFF;   // $1D the plane with an object
	uint8_t planeHitShell = 0xFF;    // $1E the plane with the tank shell (player 0)
	uint8_t shellHitObject = 0xFF;   // $1F an object with the tank shell
	uint8_t tankAtEdge = 0xFF;       // $20 the tank (player 1) with the river edge
	uint8_t missileHitPlane = 0xFF;  // $21 the helicopter's missile (missile 0) with the plane
	uint8_t missileHitBank = 0xFF;   // $22 the helicopter's missile with bank or edge
};

// Controls, as the original reads them every frame.
struct Input {
	// PORTA nibbles of joysticks 1 and 2: bit 0 up, 1 down, 2 left, 3 right; 0 = pressed
	uint8_t stick[2] = {0x0F, 0x0F};
	bool trigger[2] = {false, false};  // fire buttons
	bool start = false, select = false, option = false;
	bool key = false;                  // any key of the keyboard (pause)
};

// The data of one player ($60-$6A; the other player's copy is at $6B-$75).
struct PlayerData {
	uint8_t level = 0;          // $60 (gen.s.level for the active player)
	uint8_t bridges[2] = {};    // $61-$62 bridges destroyed, BCD
	uint8_t score[3] = {};      // $63-$65 score, BCD, high byte first
	uint8_t checkpoint[4] = {}; // $66-$69 restart point (gen.s.checkpoint for the active player)
	uint8_t lives = 0;          // $6A
};

struct GameState {
	// frame, modes, timers
	uint8_t frameCount = 0;     // $18 +1 per frame (VBI)
	uint8_t attractFlags = 0;   // $29 bit 7: attract mode (no player); bits 2-0 count frameCount wraps
	uint8_t attractTimer = 0;   // $2A counts down every 8 frames while not playing; 0 while playing
	uint8_t idleTime = 0;       // $78 +1 every 2048 frames without input; bit 7: screen off
	uint8_t pauseState = 0;     // $24 3 key pressed, 2 paused, 1 key pressed again, 0 playing
	uint8_t selectDelay = 0;    // $25 frames until SELECT/OPTION changes the game again
	uint8_t startBeep = 0;      // $C6 frames of the beep after START or SELECT
	uint8_t powerOnDone = 0;    // $28
	uint8_t screenOn = 0;       // $C7 0 while the screen is being cleared (colours off)
	// the game and the players
	uint8_t gameNumber = 0;     // $09 0..7 (shown as 1..8); odd: two players
	uint8_t player = 0;         // $77 whose turn it is (0 or 1)
	uint8_t bridges[2] = {};    // $61-$62 bridges passed, BCD
	uint8_t score[3] = {};      // $63-$65 BCD, high byte first
	uint8_t lives = 0;          // $6A planes left
	PlayerData other;           // $6B-$75 the other player in a two-player game
	uint8_t respawned = 0;      // $79 1 once a plane of this game has been lost (the panel marks the player)
	uint8_t highScore[16] = {}; // $80-$8F 3 BCD bytes for each pair of games, at 4-byte steps
	uint8_t highScoreCheck[16] = {};  // $90-$9F check bytes of the high scores (kept over a reset)
	// the plane
	// $76: 0 flying; $FE-$80 exploding; $7F-$30 river refilling; $2F waiting for the player;
	// $FF no plane (attract mode, game over)
	uint8_t planeState = 0;
	uint8_t planeX = 0;         // $57
	uint8_t planeXFrac = 0;     // $58
	uint8_t planeAccel = 0;     // $2B sideways acceleration
	uint8_t planeFrame = 0;     // $5E shape: 7/8/9 banking left / right / level, 6-0 explosion
	uint8_t speed = 0;          // $59 scrolling, (lines per frame) x 128: $40 slow .. $FE fast
	uint8_t scrollFrac = 0;     // $30
	uint8_t fuel = 0;           // $5B $FF full
	uint8_t fuelFrac = 0;       // $5C
	uint8_t bulletY = 0;        // $56 the bullet (missile 2); < $1C: no bullet
	uint8_t joystick = 0x0F;    // $19 stick bits of this frame (0 = pressed)
	uint8_t joystickPrev = 0;   // $B9 those of the previous frame, << 4
	// the tank's shell and the helicopter's missile (more in RiverState)
	uint8_t shellFrame = 0;     // $3B shape: 0 none, 1 flying, 2.. explosion
	uint8_t shellX = 0;         // $3C
	uint8_t shellXFrac = 0;     // $3D
	uint8_t shellColour = 0;    // COLPM0, set at the top of the frame (AED3)
	uint8_t missileStep = 0;    // $C0 +2 or -2 per frame
	uint8_t bridgeFlash = 0;    // $31 frames of the red water flash after a bridge is hit
	// sounds the game asks for (the VBI's sound driver plays them, counting down)
	uint8_t eventSound = 0;     // $7A channel 1: 1 crash, 2 out of fuel, 3 tank full, 4 refuelling
	uint8_t eventSoundTime = 0; // $7B its length; also the low-fuel beep counter
	uint8_t soundExplosion = 0; // $7C
	uint8_t soundMissile = 0;   // $7F
	uint8_t soundFire = 0;      // $7D
	uint8_t soundBonus = 0;     // $7E a new plane at 10 000 points
	uint8_t crashPitch = 0;     // $052C engine pitch falling after a crash
	// the screen below the river
	uint8_t panel[60] = {};     // $1000-$103B status panel characters
	Scroller scroller;          // $3FB3/$26/$27 the scrolling line below the panel (VBI B44B)
	uint8_t logoShown = 0;      // the logo position the display uses this frame
	uint16_t screenTop = 0x2000; // $3F04/$3F05 screen memory address of the top river line
	// playfield colours the VBI sets for the next frame (COLPF0 bank, COLPF1 edge, COLPF2 water)
	uint8_t colourBank = 0xD6, colourEdge = 0x20, colourWater = 0x84;
	PokeyRegs pokey;            // what the sound driver last wrote
	Collisions hit;
	PmMemory pm;
};

class Game {
public:
	explicit Game(const Rom &rom);

	RiverGen gen;     // river and object slots (gen.s)
	GameState g;

	// Lines currently in screen memory, newest first: the river area shows lines 0..159.
	std::vector<std::array<uint8_t, kLineBytes>> screen;
	// If set: the generator's state before each line in `screen`, newest first, so screen
	// memory can be made again from the oldest one (a small saved state, see compact()).
	bool keepLineStates = false;
	std::vector<RiverState> lineStates;
	// Saved states: drops screen memory (and lineStates) keeping only what makes it again, the
	// generator's state before its oldest line; expand() makes it again. The river's lines
	// depend only on the generator, never on the objects, so they come out the same.
	struct ScreenSource {
		RiverState oldest;   // the generator before the oldest line in screen memory
		uint16_t lines = 0;  // how many lines screen memory held
	};
	ScreenSource compact();
	void expand(const ScreenSource &source);
	// Called after each new river line (gen.s is the state right after it), if set.
	std::function<void(const uint8_t *line)> afterLine;

	// The state of the attract mode right after power-on, at the frame its river starts
	// (frame 38 of the original): game 1 reset (sub $B0F1 via RiverGen::startGame), no plane,
	// the first river line generated.
	void startAttract(bool pal = false);

	// Power-on (A000-A17C): clears the RAM (keeping the high scores if their check is right),
	// sets up the panel and game 1; the attract mode starts by itself a few frames later.
	void powerOn(bool pal = false);

	// One frame. random() returns the next value read from POKEY RANDOM ($D20A).
	void frame(const Input &input, const std::function<uint8_t()> &random);

	// Called in frame() right after display(), e.g. to draw the frame.
	std::function<void()> afterDisplay;
	bool busy() const { return busy_ > 0; }

	// Parts of frame(), in order (frame() also handles the frames of a reset).
	void prepare();
	void display();
	void logic(const Input &input, const std::function<uint8_t()> &random);
	void vbi(const std::function<uint8_t()> &random);
	// For jumping ahead in the attract mode (river viewer): one frame of the river only. The
	// river scrolls and the frame counters and the scrolling line advance as in frame(), but the
	// objects, the display and the sound do not run; the river (the generator) does not depend
	// on them, and the objects on screen are right again once the ones from before the jump
	// have scrolled off (a few hundred whole frames, see viewer/river_view.cpp).
	void skipFrame();
	// The sound part of the VBI (B2A5-B3D8): sets g.pokey from the game state.
	void soundDriver(const std::function<uint8_t()> &random);

	// What display() leaves for drawing: per scanline, the state of player 3 and of the bank
	// colour as the DLI set them (index = scanline, 0..261).
	struct Scanline {
		uint8_t p3 = 0, p3x = 0, p3size = 0, p3colour = 0;
		uint8_t bankColour = 0;   // COLPF0
	};
	std::array<Scanline, 262> scan{};

	// After display(): draws the river area (rows kRiverTop.. of the 384 x 240 screen buffer,
	// see status_bar.h) as the original shows it this frame: playfield, objects, the tank,
	// its shell and the helicopter missile.
	void drawRiver(uint8_t screen[240][kRowWidth]) const;

	// The state as the original keeps it in RAM ($0000-$3FFF), for the checks against traces
	// of the original (src/game_ram.cpp). Colours the VBI sets are recomputed on loading.
	void loadFromRam(const uint8_t *ram);
	void saveToRam(uint8_t *ram) const;
	// Differences from a RAM image ("name $addr: port=.. orig=.."), the screen lines included.
	std::vector<std::string> diffWithRam(const uint8_t *ram) const;

private:
	const Rom &rom_;
	bool pal_ = false;
	void mainLoop(const Input &input, const std::function<uint8_t()> &random);   // A197-A6EA
	void mainLoopEnd(const Input &input);         // A5B7-A6EA
	void finishReset(const Input &input, const std::function<uint8_t()> &random);
	void finishRespawn();                         // A3C1-A3E3
	// sub $B0F1 clears memory for this many frames; the main loop continues in the last one
	static constexpr int kResetFrames = 13;
	enum class Resume { None, NewGame, Respawn };
	Resume resume_ = Resume::None;
	int busy_ = 0;
	bool gameOverText_ = false;
	void newGame();                               // A688-A17E
	void reset(bool full);                        // sub $B0F1 ($4D-$5F, or $4D-$76 for a new game)
	void drawLogoAndNeedle();
	void selectGame();                            // A699-A6DC
	bool respawnOrGameOver();                     // A36E-A3B6
	void flyPlane(const Input &input);            // A3E9-A5B4
	void steer();                                 // A455-A496
	void changeSpeed();                           // A497-A4B5
	void crash(uint8_t sound, uint8_t length);    // sub $B1C0
	void explode(int n, uint8_t kind);            // A4E0-A589
	void removePlaneThings();                     // B204-B220
	void moveBullet(int y);                       // A5AA-A5B4
	void drawBullet(uint8_t bits);                // sub $B223
	void clearBullet();                           // sub $B221
	void swapPlayers();                           // sub $B247
	void updateHighScore(bool otherPlayer);       // sub $B261
	void showLives();                             // sub $AF5B
	void setUpGame();                             // sub $AF80
	void printBcd(const uint8_t *bcd, int bytes, int at, uint8_t digitZero);   // B086-B0C6
	void showBridges();                           // sub $B05C
	void showScore();                             // sub $B07C
	void showOtherScore();                        // sub $B068
	void showHighScore();                         // sub $B06E
	void gameOverScreen();                        // sub $B21A
	void moveObjects();                           // A26C-A2ED
	void driveTank();                             // A21B-A26A
	void moveMissile();                           // A5B7-A605
	void moveShell();                             // A607-A671
	void scrollRiver();                           // A6FD-A70F and the generator
	void pushLine();             // the generator's next line goes to the top of the screen
};

}  // namespace rr
