// River Raid game frame. See game.h. Addresses refer to the original cartridge code.
#include "game.h"

#include "objects.h"

#include <cstdlib>

#include <cstring>
#include <type_traits>

namespace rr {
namespace {

inline uint8_t lo(int v) { return uint8_t(v & 0xFF); }


// the plane's states
constexpr uint8_t kFlying = Plane::kFlying, kWaiting = Plane::kWaiting, kLifeLost = Plane::kLifeLost;
constexpr uint8_t kCrashed = Plane::kCrashed, kNoPlane = Plane::kNoPlane;

// GameState::pauseState: a key press pauses, the next press resumes
constexpr uint8_t kPlaying = 0, kResuming = 1, kPaused = 2, kPausing = 3;

// channel 1's event sounds
constexpr uint8_t kCrashSound = Sounds::kCrash, kOutOfFuelSound = Sounds::kOutOfFuel, kTankFullSound = Sounds::kTankFull, kRefuelSound = Sounds::kRefuel;

inline bool attract(const GameState &g) { return (g.attractFlags & 0x80) != 0; }

}  // namespace

Game::Runs Game::makeRuns(const uint8_t *line) {
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

Game::Game(const Rom &rom) : gen(rom), rom_(rom) {}

void Game::startAttract(bool pal) {
	pal_ = pal;
	gen.s = RiverState();
	gen.startGame(1, pal);
	g = GameState();
	screen.clear();
	bankLines.clear();
	runLines.clear();
	lineStates.clear();
	for (int i = 0; i < kPanelBytes; i++) g.panel.chars[i] = rom_.panelText(i);   // A0F0
	// the attract mode's new game: B0F1 with X = $29, AF80, then the respawn (A36E-A3E3) which
	// in the attract mode ends with sub $B21A; then frame 38
	g.attractFlags = 0x80;
	setUpGame();
	g.bullet.y = 1;
	g.lives = g.other.lives = 4;
	g.respawned = 1;
	g.powerOnDone = 1;
	g.screenOn = 1;
	g.attractFlags = 0x81;
	g.attractTimer = 0xFF;
	g.plane.state = kNoPlane;
	g.plane.speed = 0x80;
	g.plane.fuel = 0xFF;
	g.plane.fuelFrac = 0xFF;
	g.plane.x = 0x7C;
	g.colourBank = bankColour(gen.s);
	// the scrolling line has been moving since frame 11 (VBI steps up to frame 38; 9 to 25 on PAL)
	g.scroller = Scroller();
	for (int f = scrollerStartFrame(pal); f <= attractStartFrame(pal); f++) {
		g.logoShown = g.scroller.logoX;
		g.scroller.step(true);
	}
	pushLine();
}

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

static_assert(std::is_trivially_copyable_v<GameState> && std::is_trivially_copyable_v<RiverState>, "saved as bytes");

std::vector<uint8_t> Game::save() const {
	std::vector<uint8_t> raw;
	put(raw, g);
	put(raw, gen.s);
	put(raw, screen.empty() ? RiverState() : lineStates.back());   // the generator before the oldest line
	put(raw, uint16_t(screen.size()));
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
	RiverState oldest;
	uint16_t lines = 0;
	get(raw, at, g);
	get(raw, at, gen.s);
	get(raw, at, oldest);
	get(raw, at, lines);
	get(raw, at, resume_);
	get(raw, at, busy_);
	get(raw, at, gameOverText_);
	get(raw, at, pal_);
	// screen memory again, from the generator's state before its oldest line
	RiverGen run = gen;
	run.s = oldest;
	screen.clear();
	bankLines.clear();
	runLines.clear();
	lineStates.clear();
	std::array<uint8_t, kLineBytes> line, bank;
	for (int i = 0; i < lines; i++) {
		if (keepLineStates) lineStates.insert(lineStates.begin(), run.s);
		run.nextLine(line.data(), bank.data());
		screen.insert(screen.begin(), line);
		bankLines.insert(bankLines.begin(), bank);
		runLines.insert(runLines.begin(), makeRuns(bank.data()));
	}
	scan = {};
}

// A new river line goes above the others. Screen memory is a ring of two 80-line blocks,
// $2000-$2EFF and $3000-$3EFF; the address of the top line moves up by one line.
void Game::pushLine() {
	std::array<uint8_t, kLineBytes> line, bank;
	if (keepLineStates) {
		lineStates.insert(lineStates.begin(), gen.s);
		if (lineStates.size() > 256) lineStates.pop_back();
	}
	gen.nextLine(line.data(), bank.data());
	screen.insert(screen.begin(), line);
	if (screen.size() > 256) screen.pop_back();
	bankLines.insert(bankLines.begin(), bank);
	if (bankLines.size() > 256) bankLines.pop_back();
	runLines.insert(runLines.begin(), makeRuns(bank.data()));
	if (runLines.size() > 256) runLines.pop_back();
	uint8_t topLo = uint8_t(g.screenTop), topHi = uint8_t(g.screenTop >> 8);
	if (topLo < kLineBytes) topHi--;
	topLo = uint8_t(topLo - kLineBytes);
	if (topHi < 0x20) topHi = 0x3E;
	if (topHi == 0x2F) topHi = 0x2E;
	g.screenTop = uint16_t(topLo | (topHi << 8));
	if (afterLine) afterLine(line.data());
}

void Game::frame(const Input &input, const std::function<uint8_t()> &random) {
	if (busy_ > 0) {
		// sub $B0F1 is clearing memory: the main loop is stuck, only the interrupts run
		busy_--;
		if (busy_ == 1) g.screenOn = 1;           // screen memory clear: colours back on
		display();
		if (afterDisplay) afterDisplay();
		if (busy_ == 0) finishReset(input, random);
		vbi(random);
		return;
	}
	prepare();
	display();
	if (afterDisplay) afterDisplay();
	logic(input, random);
	vbi(random);
}

// AE99-AF38, at the top of the frame: the sprites shown again (the original copies the tank's,
// the shell's and the plane's shapes into player-missile memory, and the missile's row); the
// tank and the shell gone once they have scrolled below the screen; then lives, bridges and
// score into the panel.
void Game::prepare() {
	RiverState &s = gen.s;
	g.spritesShown = true;
	if (s.tank.y >= 0xBC) s.tank.kind = s.tank.y = 0;
	gen.s.shell.colour = rom_.shellColour(g.frameCount >> 3);
	if (s.shell.y >= 0xBC) {
		s.shell.timer = 0xFF;
		gen.s.shell.frame = 0;
		s.shell.y = 0;
	}
	// AF3A-AF58: lives, bridges and score into the panel
	if (g.attractTimer == 0) showLives();
	showBridges();
	if (g.player == 0) showScore();
	else showOtherScore();
}

void Game::logic(const Input &input, const std::function<uint8_t()> &random) { mainLoop(input, random); }

// The main loop continues after sub $B0F1 (13 frames after it started).
void Game::finishReset(const Input &input, const std::function<uint8_t()> &random) {
	const Resume r = resume_;
	resume_ = Resume::None;
	if (r == Resume::Respawn) {
		finishRespawn();
		mainLoopEnd(input);
	} else if (r == Resume::NewGame) {
		// AF80, A16C-A17E: the panel, then the main loop runs once more in this frame
		setUpGame();
		if (!attract(g)) {
			showLives();
			showBridges();
			showScore();
		}
		mainLoop(input, random);
	}
}

// A197-A6EA. Returns early when a reset (new game, respawn) starts: the rest runs when it ends.
void Game::mainLoop(const Input &input, const std::function<uint8_t()> &random) {
	RiverState &s = gen.s;
	// A19E: any joystick, fire or console key: not idle, and no pause
	const bool active = input.stick[0] != 0x0F || input.stick[1] != 0x0F || input.trigger[0] || input.trigger[1] ||
		input.start || input.select || input.option;
	if (active) {
		g.idleTime = 0;
		g.pauseState = kPlaying;
	}
	// A1C5-A1F3: pause with any key of the keyboard (SKSTAT bit 2): press, release (paused),
	// press, release (playing again)
	const bool keyDown = input.key;
	if (keyDown) g.idleTime = 0;
	switch (g.pauseState) {
	case kPlaying:
		if (keyDown) {
			g.pauseState = kPausing;
			return;
		}
		break;
	case kResuming:
		if (!keyDown) g.pauseState--;
		break;
	case kPaused:
		if (!keyDown) return;
		g.pauseState--;
		break;
	default:                                      // kPausing
		if (keyDown) return;
		g.pauseState--;
		return;
	}
	if (g.idleTime & 0x80) return;                // A1F5: screen off after a long time without input

	// A1FF: every 16 frames a random slot wakes up (it may move); 11-15 count as 6-10
	if ((g.frameCount & 0x0F) == 0) {
		uint8_t n = random() & 0x0F;
		if (n >= kSlots) n = lo(n - 5);
		if (n != s.enteringSlot) s.slot[n].flags |= slotflag::kAwake;
	}
	{
		RiverState &s = gen.s;
		s.tank.drive(g.frameCount, s.shell, attract(g) || g.plane.flying(), !(g.hit.tankAtEdge & 0x80), s.bridgeHit != 0, rom_);
	}
	moveObjects(gen.s, g.frameCount, attract(g) || g.plane.flying(), rom_);   // A26C

	// A2F5: the attract timer, -1 every 8 frames; when it runs out after a game, the attract
	// mode starts (with a new game 1 river)
	if (g.attractTimer && !(g.frameCount & 7)) {
		uint8_t timer = lo(g.attractTimer - 1);
		if (timer == 0) {
			timer = 0xFF;
			if (!attract(g)) {
				g.attractFlags |= 0x80;
				g.attractTimer = timer;
				newGame();
				return;
			}
		}
		g.attractTimer = timer;
	}
	// A312: the joystick of the player whose turn it is
	const int stick = g.player & 1;
	g.plane.stickPrev = lo(g.plane.stick << 4);
	g.plane.stick = input.stick[stick] & 0x0F;
	if ((g.plane.stick != 0x0F || input.trigger[stick]) && g.plane.state == kWaiting) {
		g.plane.state = kFlying;                   // A332: the waiting plane goes
		g.plane.speed = 0x40;
	}
	// A33F: the plane
	const uint8_t state = g.plane.state;
	if (state & 0x80) {                           // A35C: exploding (or no plane)
		if (state != kNoPlane) {
			if (!(lo(state + 1) & 7) && g.plane.frame) g.plane.frame--;   // explosion frames
			g.plane.state--;
			if (!(g.plane.state & 0x80) && respawnOrGameOver()) return;
		}
	} else if (state == kFlying) {
		flyPlane(input);
	} else if (state >= kLifeLost) {              // A348: the river refilling
		if (state == kLifeLost) {
			g.sound.crashPitch = 0x80;
			g.lives--;
		}
		g.plane.state--;
	}
	mainLoopEnd(input);
}

// A5B7-A6EA: the rest of the main loop.
void Game::mainLoopEnd(const Input &input) {
	{
		using namespace object;
		RiverState &s = gen.s;
		const int n = s.shooterSlot;
		const Slot *shooter = n < kSlots && (s.slot[n].kind == kShooter || s.slot[n].kind == kShooter + 1) ? &s.slot[n] : nullptr;
		if (s.missile.move(shooter, attract(g) || g.plane.flying(), !(g.hit.missileHitBank & 0x80))) g.sound.missile = 9;   // A5B7
		s.shell.move(s.tank, g.plane.state != 0, rom_);   // A607
	}

	// A673: console keys
	if (input.start) {
		g.sound.startBeep = 2;
		g.attractFlags = 0;
		g.attractTimer = 0;
		newGame();
		return;
	}
	if (input.select || input.option) {
		if (g.selectDelay-- == 0) selectGame();   // A699: every 30 frames while held
	} else {
		g.selectDelay = 0;
	}

	// A6DE: scrolling
	if (g.plane.state & 0x80) {
		if (!attract(g)) return;                  // exploding: the river stops
		g.plane.speed = g.gameNumber < 4 ? 0x80 : 0xFE; // A6F1: attract mode
	} else if (g.plane.state == kWaiting) {        // the plane level, no scrolling
		g.plane.frame = 9;
		return;
	}
	scrollRiver();
}

// A688-A17E: a new game (START), or the attract mode's river starting.
void Game::newGame() {
	g.player = g.gameNumber & 1;
	// A15D with X = $79: $79-$7F cleared
	g.respawned = 0;
	g.sound.event = g.sound.eventTime = g.sound.explosion = g.sound.fire = g.sound.bonus = g.sound.missile = 0;
	reset(true);   // the rest (AF80, A17E) when the reset ends: finishReset()
	resume_ = Resume::NewGame;
}

// sub $B0F1: reset of the river and the objects. full: a new game ($4D-$76 from the reset
// table: also level, scores, lives, the plane's state); otherwise a restart ($4D-$5F).
void Game::reset(bool full) {
	RiverState &s = gen.s;
	auto table = [&](uint16_t addr) { return rom_.resetValue(addr - 0x4D); };
	if (full) s.level = table(0x60);              // loaded before B16B reads it for the bank colour
	gen.restoreCheckpoint();   // the river's part: $4D-$5F, $2B-$4C and $0500-$05FF, the bridge in slot 10
	// (restoreCheckpoint also continues from the checkpoint, A3C8; finishRespawn() does it again)
	g.bullet.y = table(0x56);
	g.plane.x = table(0x57);
	g.plane.xFrac = table(0x58);
	g.plane.speed = table(0x59);
	g.plane.fuel = table(0x5B);
	g.plane.fuelFrac = table(0x5C);
	g.plane.frame = table(0x5E);
	if (full) {
		g.bridges = {{table(0x61), table(0x62)}};
		g.score = {{table(0x63), table(0x64), table(0x65)}};
		for (int i = 0; i < 4; i++) s.checkpoint[i] = table(uint16_t(0x66 + i));
		g.lives = table(0x6A);
		g.other.level = table(0x6B);
		g.other.bridges = {{table(0x6C), table(0x6D)}};
		g.other.score = {{table(0x6E), table(0x6F), table(0x70)}};
		for (int i = 0; i < 4; i++) g.other.checkpoint[i] = table(uint16_t(0x71 + i));
		g.other.lives = table(0x75);
		g.plane.state = table(0x76);
	}
	// $2B-$4C cleared (the river's part in restoreCheckpoint)
	g.plane.accel = 0;
	g.scrollFrac = 0;
	g.bridgeFlash = 0;
	gen.s.shell.frame = gen.s.shell.x = gen.s.shell.xFrac = 0;
	g.sound.crashPitch = 0;   // $052C ($0500-$05FF cleared)
	// player-missile memory rows 1-$BB cleared (the sprites gone until prepare()), screen memory cleared
	g.spritesShown = false;
	screen.clear();
	bankLines.clear();
	runLines.clear();
	lineStates.clear();
	g.screenOn = 0;                               // B101: colours off while clearing
	busy_ = resetFrames();
}

// sub $AF80: one- or two-player markers in the panel and the starting point of the game.
void Game::setUpGame() {
	if (g.gameNumber & 1) {
		showOtherScore();
	} else {
		showHighScore();
	}
	g.panel.markPlayers(g.gameNumber & 1);
	const RiverGen::Start &start = gen.start(g.gameNumber >> 1);
	for (int i = 0; i < 4; i++) g.other.checkpoint[i] = gen.s.checkpoint[i] = start.checkpoint[i];
	gen.s.level = g.other.level = start.level;
	// the bridge counter, 4 BCD digits: past 9999 its last 4, as the counter of a game flown that
	// far would show (it goes round; and like it, the tanks fire only from 5 on again)
	const int b = start.bridge % 10000;
	g.bridges = g.other.bridges = {{uint8_t((b / 1000) << 4 | b / 100 % 10), uint8_t((b / 10 % 10) << 4 | b % 10)}};
}

// A699-A6DC: SELECT or OPTION: the next game number, everything back to the start.
void Game::selectGame() {
	g.gameNumber = (g.gameNumber + 1) & 7;
	g.panel.showGameNumber(g.gameNumber);
	g.idleTime = 0;
	g.attractFlags = 0;
	g.lives = 0;
	g.other.lives = 0;
	g.player = 0;
	g.sound.startBeep = 2;
	g.score = g.other.score = Bcd<3>();
	g.sound.missile = 0;
	gen.s.shell.timer = 0xFF;
	setUpGame();
	showLives();
	gameOverScreen();
	g.attractTimer = 0x25;
	g.selectDelay = 0x1E;
}

// A36E-A3B6: the explosion is over: the next player, the next plane, or game over.
// Returns true when the river is reset for the next plane (the loop continues after it).
bool Game::respawnOrGameOver() {
	if (g.other.lives) swapPlayers();
	g.respawned = 1;
	if (g.lives) {
		reset(false);                             // A3BC; then finishRespawn()
		resume_ = Resume::Respawn;
		return true;
	}
	// A37D: game over
	updateHighScore(false);
	updateHighScore(true);
	if (g.gameNumber & 1) {
		if (g.player) swapPlayers();
		for (int i = 0; i < 4; i++) g.panel.chars[Panel::kLives + i] = rom_.playerTwoText(i) ^ 0xC0;
		printBcd(g.other.bridges, Panel::kOtherBridges, Panel::kDigitsOther);
	} else {
		showHighScore();
	}
	// (In the original the high score check makes this frame's work end close to the vertical
	// blank: depending on the objects on screen, sub $B21A sets the frame counter to $FF just
	// before or just after that frame's VBI counts it, or even in the next frame. The port
	// does it before the VBI, and the "Game over" text (A3B4) after it, as seen most often.
	// On PAL the panel's display interrupt keeps the CPU 12 lines longer, and the VBI comes
	// first: it still sees the frame counter, the plane and the attract timer as they were.)
	if (pal_) {
		vbiSeesBeforeGameOver_ = true;
		beforeGameOver_[0] = g.frameCount;
		beforeGameOver_[1] = g.plane.state;
		beforeGameOver_[2] = g.attractTimer;
	}
	gameOverScreen();
	gameOverText_ = true;                         // A3B4, after this frame's VBI (see vbi())
	return false;
}

// A3C1-A3E3: the river continues from the restart point, the plane in the middle of it.
void Game::finishRespawn() {
	RiverState &s = gen.s;
	s.rndA = s.checkpoint[0];
	s.rndB = s.checkpoint[1];
	s.rndC = s.checkpoint[2];
	s.bank = s.checkpoint[3];
	g.plane.x = lo(s.bank + 0x26 + s.islandWidth);
	if (attract(g)) gameOverScreen();
}

void Game::skipFrame() {
	// what prepare() does to the river's state: the tank and its shell go once they have
	// scrolled below the screen (a tank still there would keep the generator from placing the
	// next one)
	RiverState &s = gen.s;
	if (s.tank.y >= 0xBC) s.tank.kind = s.tank.y = 0;
	if (s.shell.y >= 0xBC) {
		s.shell.timer = 0xFF;
		gen.s.shell.frame = 0;
		s.shell.y = 0;
	}
	// A2F5: the attract timer (in the attract mode it only goes round)
	if (g.attractTimer && !(g.frameCount & 7)) {
		const uint8_t timer = lo(g.attractTimer - 1);
		g.attractTimer = timer ? timer : 0xFF;
	}
	scrollRiver();
	// the counters of vbi()
	g.frameCount++;
	if (g.frameCount == 0) {
		g.attractFlags = lo(g.attractFlags + 1) & 0xC7;
		if (!(g.attractFlags & 7)) {
			g.idleTime++;
			if (g.idleTime == 0) g.idleTime = 0x80;
		}
	}
	g.colourBank = bankColour(gen.s);
	g.logoShown = g.scroller.logoX;
	g.scroller.step(g.attractTimer != 0);
}

// A6FD-A70F: the speed is added twice per frame to a fraction; each carry is one new river
// line. Speeds $FE and up give a line on both steps.
void Game::scrollRiver() {
	for (int step = 0; step < 2; step++) {
		if (g.plane.speed < 0xFE) {
			const int sum = g.scrollFrac + g.plane.speed;
			g.scrollFrac = lo(sum);
			if (sum < 0x100) continue;
		}
		pushLine();
	}
}

// B2A5-B3F0: the vertical blank: the sound driver, then the frame counter.
void Game::vbi(const std::function<uint8_t()> &random) {
	if (vbiSeesBeforeGameOver_) {                 // PAL: sub $B21A comes after this VBI
		g.frameCount = beforeGameOver_[0];
		g.plane.state = beforeGameOver_[1];
		g.attractTimer = beforeGameOver_[2];
	}
	soundDriver(random);
	g.frameCount++;
	if (g.frameCount == 0) {
		g.attractFlags = lo(g.attractFlags + 1) & 0xC7;
		if (!(g.attractFlags & 7)) {
			g.idleTime++;
			if (g.idleTime == 0) g.idleTime = 0x80;
		}
	}
	// B3F2-B43E: colours for the next frame. The water flashes red while a hit bridge flashes;
	// while sub $B0F1 clears the screen the playfield colours are off.
	g.colourWater = 0x84;
	if (g.pauseState < kPaused && g.bridgeFlash) {
		if (g.bridgeFlash & 1) g.colourWater = 0x42;
		g.bridgeFlash--;
	}
	g.colourEdge = 0x20;
	g.colourBank = bankColour(gen.s);              // sub $AFD0
	if (!g.screenOn) g.colourBank = g.colourEdge = g.colourWater = 0;
	// B44B-B47F: the scrolling line moves while the game is not being played
	g.logoShown = g.scroller.logoX;
	g.scroller.step(g.attractTimer != 0);
	if (vbiSeesBeforeGameOver_) {
		vbiSeesBeforeGameOver_ = false;
		g.frameCount = 0xFF;
		g.plane.state = kNoPlane;
		g.attractTimer = 0xFF;
	}
	if (gameOverText_) {                          // A3B4: "Game over" scrolls past first
		gameOverText_ = false;
		g.scroller.textLo = 0x20;
	}
}

// B2A5-B3D8: the sound driver (sound.cpp), with what it needs of the game.
void Game::soundDriver(const std::function<uint8_t()> &random) {
	SoundContext c{g.plane};
	c.paused = g.pauseState >= kPaused;
	c.attract = attract(g);
	c.attractSound = attractSound;
	c.attractEnginePitch = attractEnginePitch;
	c.attractEngineStick = attractEngineStick;
	c.shellTimer = gen.s.shell.timer;
	g.sound.drive(g.pokey, c, random, rom_);
}

// A3E9-A5B4: the plane is flying.
void Game::flyPlane(const Input &input) {
	using namespace object;
	RiverState &s = gen.s;
	// A3E9: the plane touched an object
	const uint8_t touched = g.hit.planeHitObject;
	if (!(touched & 0x80)) {
		const uint8_t kind = s.slot[touched].kind;
		if (kind >= kJet) {
			if (kind <= kBridge) {                // an enemy or a bridge: both explode
				if (kind == kBridge) s.bridgeHit++;
				crash(kCrashSound, 0x1F);
				explode(touched, kRammed);
				return;
			}
			// A40A: a fuel depot
			const uint8_t sound = g.plane.refuel() ? kTankFullSound : kRefuelSound;
			if (sound != g.sound.event) {
				g.sound.event = sound;
				g.sound.eventTime = 8;
			}
		}
	}
	// A424: the tank shell exploding on the plane, the helicopter's missile, the bank
	const bool shellExploding = s.shell.timer >= 0x11;   // (also with no shell: timer $FF)
	if ((!(g.hit.planeHitShell & 0x80) && shellExploding) || !(g.hit.missileHitPlane & 0x80) ||
		!(g.hit.planeHitBank & 0x80)) {
		crash(kCrashSound, 0x1F);
		return;
	}
	if (!g.plane.burnFuel()) {                    // A43F
		crash(kOutOfFuelSound, 0x23);
		return;
	}
	g.plane.steer();
	g.plane.changeSpeed();
	// A4B6: the bullet hit an object
	const uint8_t shot = g.hit.bulletHitObject;
	if (!(shot & 0x80) && s.slot[shot].kind >= kJet) {
		explode(shot, kShot);
		return;
	}
	// A4C1: the tank shell's explosion destroys an object
	const uint8_t shelled = g.hit.shellHitObject;
	if (!(shelled & 0x80) && shellExploding && s.slot[shelled].kind >= kHelicopter) {
		s.slot[shelled].kind = kShot;
		g.sound.explosion = 0x17;
	}
	// A58D: the bullet stops at the bank, flies up 6 lines per frame, or is fired
	if (!(g.hit.bulletHitBank & 0x80)) {
		moveBullet(1);
	} else if (g.bullet.y >= 0x1C) {
		moveBullet(g.bullet.y - 6);
	} else if (input.trigger[g.player & 1]) {
		g.sound.fire = 0x0F;
		moveBullet(0xA9);
	} else {
		moveBullet(1);
	}
}

// A4E0-A589: the object in slot n explodes (kind kShot or kRammed); points.
void Game::explode(int n, uint8_t kind) {
	using namespace object;
	RiverState &s = gen.s;
	Slot &obj = s.slot[size_t(n)];
	obj.kind = kind;
	uint8_t type = obj.type;
	g.sound.explosion = 0x17;
	if (type == kBridge) {
		g.bridgeFlash = 0x17;
		g.bridges.add(0, 1);                      // bridge counter +1
		s.bridgeHit++;
		// A500: a tank on the bridge goes with it
		if (lo(obj.x - 8) < s.tank.x && lo(obj.x + 0x1F) >= s.tank.x) {
			s.tank.kind |= 0x80;
			g.sound.explosion = 0x37;
			type = kBridgeWithTank;
		} else if (g.bridges.d[0] != 0 || g.bridges.d[1] >= 5) {
			// A52B: from bridge 5 on, a tank close to the bridge fires
			const bool near = s.tank.kind < 6 ? lo(obj.x - s.tank.x) < 0x29 : lo(s.tank.x - obj.x) < 0x41;
			if (near) s.shell.timer++;
		}
	}
	// A547: points (BCD); every 10 000 points a new plane, up to 9
	const uint8_t tenThousands = g.score.d[1] & 0xF0;
	if (g.score.add(rom_.points(type))) {         // past 999 999: the game ends
		g.score = {{0x99, 0x99, 0x99}};
		g.lives = 0;
		crash(kCrashSound, 0x1F);
		return;
	}
	if ((g.score.d[1] & 0xF0) != tenThousands && g.lives != 9) {
		g.lives++;
		g.sound.bonus = 0x40;
	}
	moveBullet(1);                                // A589: the bullet is used up
}

// sub $B1C0: the plane crashes, with an event sound of this length.
void Game::crash(uint8_t sound, uint8_t length) {
	RiverState &s = gen.s;
	g.sound.eventTime = length;
	g.sound.event = sound;
	// where the next plane starts: past a destroyed bridge, from the next section; on the run-up
	// to a bridge not hit, one checkpoint back
	if ((s.slot[kSlots - 1].flags ^ s.blockFlags) & kBankShade) {
		if (s.bridgeHit) {
			if (s.blockFlags & kBridgeBlock) gen.sectionCheckpoint();
		} else if (!(s.blockFlags & kBridgeBlock)) {
			uint8_t level = lo(s.level - 1);
			if (level == 0x4E) level = 0x50;
			s.level = level;
			std::memcpy(s.checkpoint, s.prevCheckpoint, 4);
		}
	}
	g.plane.frame = 6;
	// B200
	g.plane.state = kCrashed;
	g.attractTimer = 0;
	removePlaneThings();
}

// sub $B21A: no plane (game over or attract mode).
void Game::gameOverScreen() {
	g.frameCount = 0xFF;
	g.plane.state = kNoPlane;
	g.attractTimer = 0xFF;
	removePlaneThings();
}

// B204-B220: the bullet and the helicopter's missile go.
void Game::removePlaneThings() {
	g.bullet.remove();
	gen.s.shooterSlot = kNoSlot;
}

// A5AA-A5B4: the bullet to row y.
void Game::moveBullet(int y) { g.bullet.moveTo(y); }

// sub $B247: two-player game: the other player's turn.
void Game::swapPlayers() {
	if (!(g.gameNumber & 1)) return;
	g.player ^= 1;
	RiverState &s = gen.s;
	PlayerData current;
	current.level = s.level;
	current.bridges = g.bridges;
	current.score = g.score;
	std::memcpy(current.checkpoint, s.checkpoint, 4);
	current.lives = g.lives;
	s.level = g.other.level;
	g.bridges = g.other.bridges;
	g.score = g.other.score;
	std::memcpy(s.checkpoint, g.other.checkpoint, 4);
	g.lives = g.other.lives;
	g.other = current;
}

// sub $B261: the score (this or the other player's) becomes the high score of this pair of
// games if not lower.
void Game::updateHighScore(bool otherPlayer) { g.highScores.update(otherPlayer ? g.other.score : g.score, g.gameNumber); }

// sub $AF5B: the lives as plane symbols in the panel (the newest blinks while the bonus sounds).
void Game::showLives() {
	int shown = g.lives;
	if (shown && (g.sound.bonus & 0x08)) shown--;
	g.panel.showLives(shown);
}

// B086-B0C6: BCD bytes as digits in the panel, leading zeros blank; nothing in the attract mode.
template <int N> void Game::printBcd(const Bcd<N> &bcd, int at, uint8_t digitZero) {
	if (!attract(g)) g.panel.print(bcd, at, digitZero);
}
void Game::showBridges() { printBcd(g.bridges, Panel::kBridges, Panel::kDigitsBridges); }
void Game::showScore() { printBcd(g.score, Panel::kScore, Panel::kDigitsScore); }
void Game::showOtherScore() { printBcd(g.score, Panel::kHighScore, Panel::kDigitsOther); }
void Game::showHighScore() { printBcd(g.highScores.of(g.gameNumber), Panel::kHighScore, Panel::kDigitsHighScore); }

// A000-A17C: power-on.
void Game::powerOn(bool pal) {
	pal_ = pal;
	gen.s = RiverState();
	gen.s.depotLevelLimit = pal ? 0x2F : 0x2E;
	g = GameState();
	screen.clear();
	bankLines.clear();
	runLines.clear();
	lineStates.clear();
	for (int i = 0; i < kPanelBytes; i++) g.panel.chars[i] = rom_.panelText(i);
	// A15D (X = $09): zero page cleared, then a new game's reset
	reset(true);
	setUpGame();
	g.powerOnDone = 1;
	g.attractTimer = 1;
	g.plane.state = kNoPlane;
}

}  // namespace rr
