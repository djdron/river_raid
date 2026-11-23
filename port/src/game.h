// River Raid game frame: what the original does once per TV frame, around the river generator.
//
// The original's main loop (A197-A6EA, AE99-AF52) and its interrupts run in this order in each
// frame, and Game::frame() follows it:
//   1. prepare(): at the top of the frame (AE99) the sprites are shown again, the tank and its
//      shell go once below the screen, the panel is updated;
//   2. display(): the screen is drawn: each object at its slot's y, the collisions as plain
//      geometry (display.cpp; the original's display list interrupt re-uses one sprite for
//      every object and reads the hardware's collision registers, docs/DLI_REMOVAL.md);
//   3. logic(): from scanline 160 (A197): object activation and movement, the tank on the
//      road and its shell, the helicopter missile, then 1 river line per frame (A6FD-A711 and
//      the river with its objects, World);
//   4. vbi(): the vertical blank interrupt (B2A5): sound driver, frame counter, timers.
//
// Everything the original does is ported: the attract mode (no player, $76 = $FF, $29 bit 7),
// starting a game (START), choosing the game (SELECT / OPTION), the plane, its speed, firing,
// fuel, collisions, explosions, score and lives, two-player games, death, respawn, game over,
// the high score of each game, pause (any key).
//
// Field comments give the original RAM address; each class lists its bytes there (mapRam), for
// comparing with traces (tools/game_check).
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "river_draw.h"
#include "bullet.h"
#include "console.h"
#include "plane.h"
#include "ram_map.h"
#include "panel.h"
#include "match.h"
#include "sound.h"
#include "river.h"
#include "river_gen.h"
#include "world.h"
#include "status_bar.h"

namespace rr {

// Collision results of one frame, set by the display (display.cpp) to the slot of the object hit
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

	void mapRam(RamMap &m) {
		m.byte("hit.bulletHitBank", 0x1A, bulletHitBank).byte("hit.planeHitBank", 0x1B, planeHitBank);
		m.byte("hit.bulletHitObject", 0x1C, bulletHitObject).byte("hit.planeHitObject", 0x1D, planeHitObject);
		m.byte("hit.planeHitShell", 0x1E, planeHitShell).byte("hit.shellHitObject", 0x1F, shellHitObject);
		m.byte("hit.tankAtEdge", 0x20, tankAtEdge).byte("hit.missileHitPlane", 0x21, missileHitPlane);
		m.byte("hit.missileHitBank", 0x22, missileHitBank);
	}
};

// Controls, as the original reads them every frame.
struct Input {
	// PORTA nibbles of joysticks 1 and 2: bit 0 up, 1 down, 2 left, 3 right; 0 = pressed
	uint8_t stick[2] = {0x0F, 0x0F};
	bool trigger[2] = {false, false};  // fire buttons
	bool start = false, select = false, option = false;
	bool key = false;                  // any key of the keyboard (pause)
};

class Game {
public:
	explicit Game(const Rom &rom);

	// The state of the attract mode right after power-on, at the frame its river starts
	// (frame 38 of the original): game 1 reset (sub $B0F1 via RiverGen::startGame), no plane,
	// the first river line generated.
	void startAttract(bool pal = false);
	// Power-on clears memory in the main loop while the interrupts go on: the scrolling line
	// moves from frame 11 and the attract mode starts at frame 38. A PAL frame is longer and
	// leaves the CPU more time, so there it takes fewer frames: 9 and 25.
	static int scrollerStartFrame(bool pal) { return pal ? 9 : 11; }
	static int attractStartFrame(bool pal) { return pal ? 25 : 38; }
	// Power-on (A000-A17C): clears the RAM (keeping the high scores if their check is right),
	// sets up the panel and game 1; the attract mode starts by itself a few frames later.
	void powerOn(bool pal = false);
	// The river's starting points: the river from seed (0: the original's), with bridge as the
	// first bridge of game 1 (0: the original's); before startAttract() / powerOn().
	void setRiver(uint32_t seed, int bridge) { world_.setRiver(seed, bridge); }

	// One frame. random() returns the next value read from POKEY RANDOM ($D20A).
	void frame(const Input &input, const std::function<uint8_t()> &random);
	// Called in frame() right after display(), e.g. to draw the frame.
	std::function<void()> afterDisplay;
	// Called after each new river line (world().gen.s is the state right after it), if set.
	std::function<void(const uint8_t *line)> afterLine;

	// What a frame shows and plays. The river area: drawRiver(), after display(): rows
	// kRiverTop.. of the 384 x 240 screen buffer (see status_bar.h) as the original shows it this
	// frame (playfield, objects, the tank, its shell and the helicopter missile). Below it:
	// the status bar (drawStatusBar) from the panel, the fuel, the scrolling line and the logo.
	void drawRiver(uint8_t screen[240][kRowWidth]) const;
	struct Output {
		const Panel &panel;
		uint8_t fuel;
		const Scroller &scroller;
		uint8_t logoX;             // the logo position the display uses this frame
		bool screenOff;            // idle too long: the original turns the screen off
		const PokeyRegs &pokey;    // the sound registers
	};
	Output output() const { return {panel_, plane_.fuel, scroller_, logoShown_, console_.screenOff(), pokey_}; }

	// Reading the state (field comments in the private part give the original's RAM addresses)
	const World &world() const { return world_; }
	const River &river() const { return river_; }
	const Match &match() const { return match_; }
	const Plane &plane() const { return plane_; }
	const Bullet &bullet() const { return bullet_; }
	const Sounds &sound() const { return sound_; }
	const PokeyRegs &pokey() const { return pokey_; }
	const Collisions &hit() const { return hit_; }
	const Panel &panel() const { return panel_; }
	const Scroller &scroller() const { return scroller_; }
	uint8_t logoShown() const { return logoShown_; }
	const Console &console() const { return console_; }
	uint8_t scrollFrac() const { return scrollFrac_; }
	bool busy() const { return busy_ > 0; }
	// The attract mode: the game plays itself, nobody has pressed START
	bool attract() const { return console_.attract(); }

	// A saved state (needs keepLineStates): only what the game cannot make again. The variables
	// (the state's members, eachState) and the river's and objects' state are kept; screen
	// memory is kept as that state before its oldest line and the number of lines, and made
	// again on loading (the river's lines depend only on it); the per-scanline data (scan) is
	// made again by the next frame's display(). Runs of zero bytes are packed, so a saved state
	// is well under 1 KB. load() needs the same starting points (setRiver).
	std::vector<uint8_t> save() const;
	void load(const std::vector<uint8_t> &data);
	// The river's state before each line on the screen, for saved states (about 33 KB)
	void keepLineStates(bool keep) { river_.keepLineStates = keep; }

	// --- For the river viewer and the tools; the game itself (game/river_raid.cpp) uses none
	// of these.

	// Parts of frame(), in order (frame() also handles the frames of a reset), for the river
	// viewer, which runs them one by one, and sound_check (the driver alone).
	void prepare();
	// Each object at its slot's y, the collisions as plain geometry (display.cpp)
	void display();
	void logic(const Input &input, const std::function<uint8_t()> &random);
	void vbi(const std::function<uint8_t()> &random);
	// The sound part of the VBI (B2A5-B3D8): sets pokey() from the game state.
	void soundDriver(const std::function<uint8_t()> &random);

	// For jumping ahead in the attract mode (river viewer): one frame of the river only. The
	// river scrolls and the frame counters and the scrolling line advance as in frame(), but the
	// objects, the display and the sound do not run; the river (the generator) does not depend
	// on them, and the objects on screen are right again once the ones from before the jump
	// have scrolled off (a few hundred whole frames, see viewer/river_view.cpp).
	void skipFrame();
	// What skipFrame() changes: the river and its objects, the counters, the scrolling line.
	struct SkipState {
		World::State world;
		uint8_t frameCount, attractFlags, attractTimer, scrollFrac, logoShown;
		Scroller scroller;
		uint16_t screenTop;
	};
	SkipState skipState() const {
		return {world_.state(), console_.frameCount, console_.attractFlags, console_.attractTimer, scrollFrac_, logoShown_, scroller_, river_.top};
	}
	void setSkipState(const SkipState &s);

	// Not in the original: the attract mode with sound (AttractSound, sound.h)
	AttractSound attractSound;
	// The river viewer's sound keys: a sound started as the game starts it
	enum class Sound { Explosion, Fire, Bonus };
	void playSound(Sound s);
	// The river viewer keeps the screen on (no idle time)
	void stayAwake() { console_.idleTime = 0; }

	// For the tools' ghost mode (game_trace, display_bench -ghost), from afterDisplay: the plane
	// is not hit this frame and its tank stays full.
	void ghost() {
		hit_.planeHitBank = hit_.planeHitObject = hit_.planeHitShell = hit_.missileHitPlane = 0xFF;
		plane_.fuel = 0xFF;
	}

	// The state as the original keeps it in RAM ($0000-$3FFF), for the checks against traces
	// of the original (src/game_ram.cpp): the game's own bytes and those of its parts.
	void mapRam(RamMap &m);
	// Colours the VBI sets are recomputed on loading.
	void loadFromRam(const uint8_t *ram);
	void saveToRam(uint8_t *ram) const;
	// Differences from a RAM image ("name $addr: port=.. orig=.."), the screen lines included.
	std::vector<std::string> diffWithRam(const uint8_t *ram) const;
	// The sound registers as the original last wrote them (sound_check: before its driver runs)
	void setPokey(const PokeyRegs &regs) { pokey_ = regs; }

private:
	World world_;                 // the river and the objects on it
	River river_;                 // the lines on the screen

	// The state of the game (field comments give the original's RAM address)

	Console console_;             // frame counter, attract mode, idle time, pause, SELECT: $18, $24-$2A, $78, $C7
	Match match_;                 // the game and the players: $09, $61-$75, $77, $79, $80-$9F
	Plane plane_;                 // $76, $57-$59, $5B-$5C, $5E, $2B, $19, $B9
	Bullet bullet_;               // $56 the plane's bullet
	uint8_t scrollFrac_ = 0;      // $30 the river's scrolling: the plane's speed added to it twice a frame
	uint8_t bridgeFlash_ = 0;     // $31 frames of the red water flash after a bridge is hit
	Sounds sound_;                // the sounds the game asks for ($7A-$7F, $C6, $052C)
	// the screen below the river
	Panel panel_;                 // $1000-$103B the status panel's characters
	Scroller scroller_;           // $3FB3/$26/$27 the scrolling line below the panel (VBI B44B)
	uint8_t logoShown_ = 0;       // the logo position the display uses this frame
	// playfield colours the VBI sets for the next frame (COLPF0 bank, COLPF1 edge, COLPF2 water)
	uint8_t colourBank_ = 0xD6, colourEdge_ = 0x20, colourWater_ = 0x84;
	PokeyRegs pokey_;             // what the sound driver last wrote
	Collisions hit_;
	// The sprites (shell, tank, plane, missile, bullet) are shown from their state; a reset
	// (sub $B0F1) clears them until the next frame's prepare() (AE99) draws them again.
	bool spritesShown_ = false;

	// What display() leaves for drawing: per scanline, the bank colour (COLPF0: a bridge's road
	// has its own; index = scanline, 0..261) ...
	struct Scanline {
		uint8_t bankColour = 0;
	};
	std::array<Scanline, 262> scan_{};
	// ... and each object's first scanline, position, size, rows and their colours (the first
	// row on the first scanline). Fixed arrays, so that a frame allocates nothing: an object has
	// at most $80 rows (display.cpp). The vector keeps its capacity from frame to frame.
	struct DrawnObject {
		static constexpr int kMaxRows = 0x80;
		int slot = 0, first = 0;
		uint8_t x = 0, size = 0;
		bool edge = false;        // tested against the river's edge (turnsAtEdge)
		bool bridge = false;
		int count = 0;            // rows
		std::array<uint8_t, kMaxRows> rows{}, colours{};
	};
	std::vector<DrawnObject> drawn_;
	// A sprite's graphics bits on a scanline: 0 the shell, 1 the tank, 2 the plane (players,
	// 8 bits), 4 the helicopter's missile, 5 the bullet (missiles, 2 bits)
	uint8_t spriteRow(int sprite, int line) const;

	// All the state as after construction (power-on, the attract mode's start)
	void resetState();
	// The state's members, in order, for saving, loading and resetting them
	template <class F> void eachState(F f) { eachStateOf(*this, f); }
	template <class F> void eachState(F f) const { eachStateOf(*this, f); }
	template <class Self, class F> static void eachStateOf(Self &s, F f) {
		f(s.console_);
		f(s.match_);
		f(s.plane_);
		f(s.bullet_);
		f(s.scrollFrac_);
		f(s.bridgeFlash_);
		f(s.sound_);
		f(s.panel_);
		f(s.scroller_);
		f(s.logoShown_);
		f(s.colourBank_);
		f(s.colourEdge_);
		f(s.colourWater_);
		f(s.pokey_);
		f(s.hit_);
		f(s.spritesShown_);
	}
	const Rom &rom_;
	bool pal_ = false;
	void mainLoop(const Input &input, const std::function<uint8_t()> &random);   // A197-A6EA
	void mainLoopEnd(const Input &input);         // A5B7-A6EA
	// the main loop's steps, in order
	static bool active(const Input &input);       // A19E
	void wakeObject(const std::function<uint8_t()> &random);   // A1FF
	void driveTank();                             // A21B
	bool attractTimerRunsOut();                   // A2F5
	void readStick(const Input &input);           // A312
	bool updatePlane(const Input &input);         // A33F
	void moveMissile();                           // A5B7
	void moveShell();                             // A607
	bool consoleKeys(const Input &input);         // A673
	void scroll();                                // A6DE
	void finishReset(const Input &input, const std::function<uint8_t()> &random);
	void finishRespawn();                         // A3C1-A3E3
	// sub $B0F1 clears memory for this many frames; the main loop continues in the last one.
	// A PAL frame leaves the CPU more time, so there it takes fewer.
	int resetFrames() const { return pal_ ? 8 : 13; }
	enum class Resume { None, NewGame, Respawn };
	Resume resume_ = Resume::None;
	int busy_ = 0;
	bool gameOverText_ = false;
	bool vbiSeesBeforeGameOver_ = false;          // PAL: see respawnOrGameOver()
	uint8_t beforeGameOver_[3] = {};
	void newGame();                               // A688-A17E
	void reset(bool full);                        // sub $B0F1 ($4D-$5F, or $4D-$76 for a new game)
	void selectGame();                            // A699-A6DC
	bool respawnOrGameOver();                     // A36E-A3B6
	void flyPlane(const Input &input);            // A3E9-A5B4
	void crash(uint8_t event, uint8_t length);    // sub $B1C0
	void explode(int n, uint8_t kind);            // A4E0-A589
	void removePlaneThings();                     // B204-B220
	void moveBullet(int y);                       // A5AA-A5B4
	void setUpGame();                             // sub $AF80 (Match::setUp)
	void showLives();                             // sub $AF5B (Scoreboard::lives)
	Scoreboard scoreboard() { return {match_, panel_, attract()}; }   // the match in the panel
	void gameOverScreen();                        // sub $B21A
	void scrollRiver();                           // A6FD-A70F and the generator
	void pushLine();             // the generator's next line goes to the top of the screen
};

}  // namespace rr
