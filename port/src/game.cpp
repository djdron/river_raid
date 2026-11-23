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

// pauseState: a key press pauses, the next press resumes
constexpr uint8_t kPlaying = 0, kResuming = 1, kPaused = 2, kPausing = 3;

// channel 1's event sounds
constexpr uint8_t kCrashSound = Sounds::kCrash, kOutOfFuelSound = Sounds::kOutOfFuel, kTankFullSound = Sounds::kTankFull, kRefuelSound = Sounds::kRefuel;

inline bool attract(const Game &g) { return (g.attractFlags & 0x80) != 0; }

}  // namespace

Game::Game(const Rom &rom) : world(rom), rom_(rom) {}

void Game::startAttract(bool pal) {
	pal_ = pal;
	world.gen.s = RiverState();
	world.startGame(1, pal);
	resetState();
	river.clear();
	for (int i = 0; i < kPanelBytes; i++) panel.chars[i] = rom_.panelText(i);   // A0F0
	// the attract mode's new game: B0F1 with X = $29, AF80, then the respawn (A36E-A3E3) which
	// in the attract mode ends with sub $B21A; then frame 38
	attractFlags = 0x80;
	setUpGame();
	bullet.y = 1;
	lives = other.lives = 4;
	respawned = 1;
	powerOnDone = 1;
	screenOn = 1;
	attractFlags = 0x81;
	attractTimer = 0xFF;
	plane.state = kNoPlane;
	plane.speed = 0x80;
	plane.fuel = 0xFF;
	plane.fuelFrac = 0xFF;
	plane.x = 0x7C;
	colourBank = bankColour(world.gen.s, world.objects);
	// the scrolling line has been moving since frame 11 (VBI steps up to frame 38; 9 to 25 on PAL)
	scroller = Scroller();
	for (int f = scrollerStartFrame(pal); f <= attractStartFrame(pal); f++) {
		logoShown = scroller.logoX;
		scroller.step(true);
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

static_assert(std::is_trivially_copyable_v<World::State>, "saved as bytes");

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
		static_assert(std::is_trivially_copyable_v<std::decay_t<decltype(x)>>, "saved as bytes");
		put(raw, x);
	});
	put(raw, world.state());
	put(raw, river.lines.empty() ? World::State() : river.lineStates.back());   // the river and the objects before the oldest line
	put(raw, uint16_t(river.lines.size()));
	put(raw, river.top);
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
	world.setState(now);
	get(raw, at, oldest);
	get(raw, at, lines);
	get(raw, at, river.top);
	get(raw, at, resume_);
	get(raw, at, busy_);
	get(raw, at, gameOverText_);
	get(raw, at, pal_);
	river.rebuild(world, oldest, lines);   // screen memory again, from the generator's state before its oldest line
	scan = {};
}

// A new river line goes above the others.
void Game::pushLine() {
	river.push(world);
	if (afterLine) afterLine(river.lines.front().data());
}

void Game::frame(const Input &input, const std::function<uint8_t()> &random) {
	if (busy_ > 0) {
		// sub $B0F1 is clearing memory: the main loop is stuck, only the interrupts run
		busy_--;
		if (busy_ == 1) screenOn = 1;           // screen memory clear: colours back on
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
	Objects &objs = world.objects;
	spritesShown = true;
	if (objs.tank.y >= 0xBC) objs.tank.kind = objs.tank.y = 0;
	objs.shell.colour = rom_.shellColour(frameCount >> 3);
	if (objs.shell.y >= 0xBC) {
		objs.shell.timer = 0xFF;
		objs.shell.frame = 0;
		objs.shell.y = 0;
	}
	// AF3A-AF58: lives, bridges and score into the panel
	if (attractTimer == 0) showLives();
	showBridges();
	if (player == 0) showScore();
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
		if (!attract(*this)) {
			showLives();
			showBridges();
			showScore();
		}
		mainLoop(input, random);
	}
}

// A197-A6EA. Returns early when a reset (new game, respawn) starts: the rest runs when it ends.
void Game::mainLoop(const Input &input, const std::function<uint8_t()> &random) {
	Objects &objs = world.objects;
	// A19E: any joystick, fire or console key: not idle, and no pause
	const bool active = input.stick[0] != 0x0F || input.stick[1] != 0x0F || input.trigger[0] || input.trigger[1] ||
		input.start || input.select || input.option;
	if (active) {
		idleTime = 0;
		pauseState = kPlaying;
	}
	// A1C5-A1F3: pause with any key of the keyboard (SKSTAT bit 2): press, release (paused),
	// press, release (playing again)
	const bool keyDown = input.key;
	if (keyDown) idleTime = 0;
	switch (pauseState) {
	case kPlaying:
		if (keyDown) {
			pauseState = kPausing;
			return;
		}
		break;
	case kResuming:
		if (!keyDown) pauseState--;
		break;
	case kPaused:
		if (!keyDown) return;
		pauseState--;
		break;
	default:                                      // kPausing
		if (keyDown) return;
		pauseState--;
		return;
	}
	if (idleTime & 0x80) return;                // A1F5: screen off after a long time without input

	// A1FF: every 16 frames a random slot wakes up (it may move); 11-15 count as 6-10
	if ((frameCount & 0x0F) == 0) {
		uint8_t n = random() & 0x0F;
		if (n >= kSlots) n = lo(n - 5);
		if (n != objs.enteringSlot) objs.slot[n].flags |= slotflag::kAwake;
	}
	{
		RiverState &s = world.gen.s;
		Objects &objs = world.objects;
		objs.tank.drive(frameCount, objs.shell, attract(*this) || plane.flying(), !(hit.tankAtEdge & 0x80), s.bridgeHit != 0, rom_);
	}
	moveObjects(world.objects, world.gen.s.level, frameCount, attract(*this) || plane.flying(), rom_);   // A26C

	// A2F5: the attract timer, -1 every 8 frames; when it runs out after a game, the attract
	// mode starts (with a new game 1 river)
	if (attractTimer && !(frameCount & 7)) {
		uint8_t timer = lo(attractTimer - 1);
		if (timer == 0) {
			timer = 0xFF;
			if (!attract(*this)) {
				attractFlags |= 0x80;
				attractTimer = timer;
				newGame();
				return;
			}
		}
		attractTimer = timer;
	}
	// A312: the joystick of the player whose turn it is
	const int stick = player & 1;
	plane.stickPrev = lo(plane.stick << 4);
	plane.stick = input.stick[stick] & 0x0F;
	if ((plane.stick != 0x0F || input.trigger[stick]) && plane.state == kWaiting) {
		plane.state = kFlying;                   // A332: the waiting plane goes
		plane.speed = 0x40;
	}
	// A33F: the plane
	const uint8_t state = plane.state;
	if (state & 0x80) {                           // A35C: exploding (or no plane)
		if (state != kNoPlane) {
			if (!(lo(state + 1) & 7) && plane.frame) plane.frame--;   // explosion frames
			plane.state--;
			if (!(plane.state & 0x80) && respawnOrGameOver()) return;
		}
	} else if (state == kFlying) {
		flyPlane(input);
	} else if (state >= kLifeLost) {              // A348: the river refilling
		if (state == kLifeLost) {
			sound.crashPitch = 0x80;
			lives--;
		}
		plane.state--;
	}
	mainLoopEnd(input);
}

// A5B7-A6EA: the rest of the main loop.
void Game::mainLoopEnd(const Input &input) {
	{
		using namespace object;
		Objects &objs = world.objects;
		const int n = objs.shooterSlot;
		const Slot *shooter = n < kSlots && (objs.slot[n].kind == kShooter || objs.slot[n].kind == kShooter + 1) ? &objs.slot[n] : nullptr;
		if (objs.missile.move(shooter, attract(*this) || plane.flying(), !(hit.missileHitBank & 0x80))) sound.missile = 9;   // A5B7
		objs.shell.move(objs.tank, plane.state != 0, rom_);   // A607
	}

	// A673: console keys
	if (input.start) {
		sound.startBeep = 2;
		attractFlags = 0;
		attractTimer = 0;
		newGame();
		return;
	}
	if (input.select || input.option) {
		if (selectDelay-- == 0) selectGame();   // A699: every 30 frames while held
	} else {
		selectDelay = 0;
	}

	// A6DE: scrolling
	if (plane.state & 0x80) {
		if (!attract(*this)) return;                  // exploding: the river stops
		plane.speed = gameNumber < 4 ? 0x80 : 0xFE; // A6F1: attract mode
	} else if (plane.state == kWaiting) {        // the plane level, no scrolling
		plane.frame = 9;
		return;
	}
	scrollRiver();
}

// A688-A17E: a new game (START), or the attract mode's river starting.
void Game::newGame() {
	player = gameNumber & 1;
	// A15D with X = $79: $79-$7F cleared
	respawned = 0;
	sound.event = sound.eventTime = sound.explosion = sound.fire = sound.bonus = sound.missile = 0;
	reset(true);   // the rest (AF80, A17E) when the reset ends: finishReset()
	resume_ = Resume::NewGame;
}

// sub $B0F1: reset of the river and the objects. full: a new game ($4D-$76 from the reset
// table: also level, scores, lives, the plane's state); otherwise a restart ($4D-$5F).
void Game::reset(bool full) {
	RiverState &s = world.gen.s;
	auto table = [&](uint16_t addr) { return rom_.resetValue(addr - 0x4D); };
	if (full) s.level = table(0x60);              // loaded before B16B reads it for the bank colour
	world.restoreCheckpoint();   // the river's part: $4D-$5F, $2B-$4C and $0500-$05FF, the bridge in slot 10
	// (restoreCheckpoint also continues from the checkpoint, A3C8; finishRespawn() does it again)
	bullet.y = table(0x56);
	plane.x = table(0x57);
	plane.xFrac = table(0x58);
	plane.speed = table(0x59);
	plane.fuel = table(0x5B);
	plane.fuelFrac = table(0x5C);
	plane.frame = table(0x5E);
	if (full) {
		bridges = {{table(0x61), table(0x62)}};
		score = {{table(0x63), table(0x64), table(0x65)}};
		for (int i = 0; i < 4; i++) s.checkpoint[i] = table(uint16_t(0x66 + i));
		lives = table(0x6A);
		other.level = table(0x6B);
		other.bridges = {{table(0x6C), table(0x6D)}};
		other.score = {{table(0x6E), table(0x6F), table(0x70)}};
		for (int i = 0; i < 4; i++) other.checkpoint[i] = table(uint16_t(0x71 + i));
		other.lives = table(0x75);
		plane.state = table(0x76);
	}
	// $2B-$4C cleared (the river's part in restoreCheckpoint)
	plane.accel = 0;
	scrollFrac = 0;
	bridgeFlash = 0;
	world.objects.shell.frame = world.objects.shell.x = world.objects.shell.xFrac = 0;
	sound.crashPitch = 0;   // $052C ($0500-$05FF cleared)
	// player-missile memory rows 1-$BB cleared (the sprites gone until prepare()), screen memory cleared
	spritesShown = false;
	river.clear();
	screenOn = 0;                               // B101: colours off while clearing
	busy_ = resetFrames();
}

// sub $AF80: one- or two-player markers in the panel and the starting point of the game.
void Game::setUpGame() {
	if (gameNumber & 1) {
		showOtherScore();
	} else {
		showHighScore();
	}
	panel.markPlayers(gameNumber & 1);
	const RiverGen::Start &start = world.gen.start(gameNumber >> 1);
	for (int i = 0; i < 4; i++) other.checkpoint[i] = world.gen.s.checkpoint[i] = start.checkpoint[i];
	world.gen.s.level = other.level = start.level;
	// the bridge counter, 4 BCD digits: past 9999 its last 4, as the counter of a game flown that
	// far would show (it goes round; and like it, the tanks fire only from 5 on again)
	const int b = start.bridge % 10000;
	bridges = other.bridges = {{uint8_t((b / 1000) << 4 | b / 100 % 10), uint8_t((b / 10 % 10) << 4 | b % 10)}};
}

// A699-A6DC: SELECT or OPTION: the next game number, everything back to the start.
void Game::selectGame() {
	gameNumber = (gameNumber + 1) & 7;
	panel.showGameNumber(gameNumber);
	idleTime = 0;
	attractFlags = 0;
	lives = 0;
	other.lives = 0;
	player = 0;
	sound.startBeep = 2;
	score = other.score = Bcd<3>();
	sound.missile = 0;
	world.objects.shell.timer = 0xFF;
	setUpGame();
	showLives();
	gameOverScreen();
	attractTimer = 0x25;
	selectDelay = 0x1E;
}

// A36E-A3B6: the explosion is over: the next player, the next plane, or game over.
// Returns true when the river is reset for the next plane (the loop continues after it).
bool Game::respawnOrGameOver() {
	if (other.lives) swapPlayers();
	respawned = 1;
	if (lives) {
		reset(false);                             // A3BC; then finishRespawn()
		resume_ = Resume::Respawn;
		return true;
	}
	// A37D: game over
	updateHighScore(false);
	updateHighScore(true);
	if (gameNumber & 1) {
		if (player) swapPlayers();
		for (int i = 0; i < 4; i++) panel.chars[Panel::kLives + i] = rom_.playerTwoText(i) ^ 0xC0;
		printBcd(other.bridges, Panel::kOtherBridges, Panel::kDigitsOther);
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
		beforeGameOver_[0] = frameCount;
		beforeGameOver_[1] = plane.state;
		beforeGameOver_[2] = attractTimer;
	}
	gameOverScreen();
	gameOverText_ = true;                         // A3B4, after this frame's VBI (see vbi())
	return false;
}

// A3C1-A3E3: the river continues from the restart point, the plane in the middle of it.
void Game::finishRespawn() {
	RiverState &s = world.gen.s;
	s.rndA = s.checkpoint[0];
	s.rndB = s.checkpoint[1];
	s.rndC = s.checkpoint[2];
	s.bank = s.checkpoint[3];
	plane.x = lo(s.bank + 0x26 + s.islandWidth);
	if (attract(*this)) gameOverScreen();
}

void Game::skipFrame() {
	// what prepare() does to the river's state: the tank and its shell go once they have
	// scrolled below the screen (a tank still there would keep the generator from placing the
	// next one)
	Objects &objs = world.objects;
	if (objs.tank.y >= 0xBC) objs.tank.kind = objs.tank.y = 0;
	if (objs.shell.y >= 0xBC) {
		objs.shell.timer = 0xFF;
		objs.shell.frame = 0;
		objs.shell.y = 0;
	}
	// A2F5: the attract timer (in the attract mode it only goes round)
	if (attractTimer && !(frameCount & 7)) {
		const uint8_t timer = lo(attractTimer - 1);
		attractTimer = timer ? timer : 0xFF;
	}
	scrollRiver();
	// the counters of vbi()
	frameCount++;
	if (frameCount == 0) {
		attractFlags = lo(attractFlags + 1) & 0xC7;
		if (!(attractFlags & 7)) {
			idleTime++;
			if (idleTime == 0) idleTime = 0x80;
		}
	}
	colourBank = bankColour(world.gen.s, world.objects);
	logoShown = scroller.logoX;
	scroller.step(attractTimer != 0);
}

// A6FD-A70F: the speed is added twice per frame to a fraction; each carry is one new river
// line. Speeds $FE and up give a line on both steps.
void Game::scrollRiver() {
	for (int step = 0; step < 2; step++) {
		if (plane.speed < 0xFE) {
			const int sum = scrollFrac + plane.speed;
			scrollFrac = lo(sum);
			if (sum < 0x100) continue;
		}
		pushLine();
	}
}

// B2A5-B3F0: the vertical blank: the sound driver, then the frame counter.
void Game::vbi(const std::function<uint8_t()> &random) {
	if (vbiSeesBeforeGameOver_) {                 // PAL: sub $B21A comes after this VBI
		frameCount = beforeGameOver_[0];
		plane.state = beforeGameOver_[1];
		attractTimer = beforeGameOver_[2];
	}
	soundDriver(random);
	frameCount++;
	if (frameCount == 0) {
		attractFlags = lo(attractFlags + 1) & 0xC7;
		if (!(attractFlags & 7)) {
			idleTime++;
			if (idleTime == 0) idleTime = 0x80;
		}
	}
	// B3F2-B43E: colours for the next frame. The water flashes red while a hit bridge flashes;
	// while sub $B0F1 clears the screen the playfield colours are off.
	colourWater = 0x84;
	if (pauseState < kPaused && bridgeFlash) {
		if (bridgeFlash & 1) colourWater = 0x42;
		bridgeFlash--;
	}
	colourEdge = 0x20;
	colourBank = bankColour(world.gen.s, world.objects);              // sub $AFD0
	if (!screenOn) colourBank = colourEdge = colourWater = 0;
	// B44B-B47F: the scrolling line moves while the game is not being played
	logoShown = scroller.logoX;
	scroller.step(attractTimer != 0);
	if (vbiSeesBeforeGameOver_) {
		vbiSeesBeforeGameOver_ = false;
		frameCount = 0xFF;
		plane.state = kNoPlane;
		attractTimer = 0xFF;
	}
	if (gameOverText_) {                          // A3B4: "Game over" scrolls past first
		gameOverText_ = false;
		scroller.textLo = 0x20;
	}
}

// B2A5-B3D8: the sound driver (sound.cpp), with what it needs of the game.
void Game::soundDriver(const std::function<uint8_t()> &random) {
	SoundContext c{plane};
	c.paused = pauseState >= kPaused;
	c.attract = attract(*this);
	c.attractSound = attractSound;
	c.attractEnginePitch = attractEnginePitch;
	c.attractEngineStick = attractEngineStick;
	c.shellTimer = world.objects.shell.timer;
	sound.drive(pokey, c, random, rom_);
}

// A3E9-A5B4: the plane is flying.
void Game::flyPlane(const Input &input) {
	using namespace object;
	RiverState &s = world.gen.s;
	Objects &objs = world.objects;
	// A3E9: the plane touched an object
	const uint8_t touched = hit.planeHitObject;
	if (!(touched & 0x80)) {
		const uint8_t kind = objs.slot[touched].kind;
		if (kind >= kJet) {
			if (kind <= kBridge) {                // an enemy or a bridge: both explode
				if (kind == kBridge) s.bridgeHit++;
				crash(kCrashSound, 0x1F);
				explode(touched, kRammed);
				return;
			}
			// A40A: a fuel depot
			const uint8_t event = plane.refuel() ? kTankFullSound : kRefuelSound;
			if (event != sound.event) {
				sound.event = event;
				sound.eventTime = 8;
			}
		}
	}
	// A424: the tank shell exploding on the plane, the helicopter's missile, the bank
	const bool shellExploding = objs.shell.timer >= 0x11;   // (also with no shell: timer $FF)
	if ((!(hit.planeHitShell & 0x80) && shellExploding) || !(hit.missileHitPlane & 0x80) ||
		!(hit.planeHitBank & 0x80)) {
		crash(kCrashSound, 0x1F);
		return;
	}
	if (!plane.burnFuel()) {                    // A43F
		crash(kOutOfFuelSound, 0x23);
		return;
	}
	plane.steer();
	plane.changeSpeed();
	// A4B6: the bullet hit an object
	const uint8_t shot = hit.bulletHitObject;
	if (!(shot & 0x80) && objs.slot[shot].kind >= kJet) {
		explode(shot, kShot);
		return;
	}
	// A4C1: the tank shell's explosion destroys an object
	const uint8_t shelled = hit.shellHitObject;
	if (!(shelled & 0x80) && shellExploding && objs.slot[shelled].kind >= kHelicopter) {
		objs.slot[shelled].kind = kShot;
		sound.explosion = 0x17;
	}
	// A58D: the bullet stops at the bank, flies up 6 lines per frame, or is fired
	if (!(hit.bulletHitBank & 0x80)) {
		moveBullet(1);
	} else if (bullet.y >= 0x1C) {
		moveBullet(bullet.y - 6);
	} else if (input.trigger[player & 1]) {
		sound.fire = 0x0F;
		moveBullet(0xA9);
	} else {
		moveBullet(1);
	}
}

// A4E0-A589: the object in slot n explodes (kind kShot or kRammed); points.
void Game::explode(int n, uint8_t kind) {
	using namespace object;
	RiverState &s = world.gen.s;
	Objects &objs = world.objects;
	Slot &obj = objs.slot[size_t(n)];
	obj.kind = kind;
	uint8_t type = obj.type;
	sound.explosion = 0x17;
	if (type == kBridge) {
		bridgeFlash = 0x17;
		bridges.add(0, 1);                      // bridge counter +1
		s.bridgeHit++;
		// A500: a tank on the bridge goes with it
		if (lo(obj.x - 8) < objs.tank.x && lo(obj.x + 0x1F) >= objs.tank.x) {
			objs.tank.kind |= 0x80;
			sound.explosion = 0x37;
			type = kBridgeWithTank;
		} else if (bridges.d[0] != 0 || bridges.d[1] >= 5) {
			// A52B: from bridge 5 on, a tank close to the bridge fires
			const bool near = objs.tank.kind < 6 ? lo(obj.x - objs.tank.x) < 0x29 : lo(objs.tank.x - obj.x) < 0x41;
			if (near) objs.shell.timer++;
		}
	}
	// A547: points (BCD); every 10 000 points a new plane, up to 9
	const uint8_t tenThousands = score.d[1] & 0xF0;
	if (score.add(rom_.points(type))) {         // past 999 999: the game ends
		score = {{0x99, 0x99, 0x99}};
		lives = 0;
		crash(kCrashSound, 0x1F);
		return;
	}
	if ((score.d[1] & 0xF0) != tenThousands && lives != 9) {
		lives++;
		sound.bonus = 0x40;
	}
	moveBullet(1);                                // A589: the bullet is used up
}

// sub $B1C0: the plane crashes, with an event sound of this length.
void Game::crash(uint8_t event, uint8_t length) {
	RiverState &s = world.gen.s;
	Objects &objs = world.objects;
	sound.eventTime = length;
	sound.event = event;
	// where the next plane starts: past a destroyed bridge, from the next section; on the run-up
	// to a bridge not hit, one checkpoint back
	if ((objs.slot[kSlots - 1].flags ^ s.blockFlags) & kBankShade) {
		if (s.bridgeHit) {
			if (s.blockFlags & kBridgeBlock) world.gen.sectionCheckpoint();
		} else if (!(s.blockFlags & kBridgeBlock)) {
			uint8_t level = lo(s.level - 1);
			if (level == 0x4E) level = 0x50;
			s.level = level;
			std::memcpy(s.checkpoint, s.prevCheckpoint, 4);
		}
	}
	plane.frame = 6;
	// B200
	plane.state = kCrashed;
	attractTimer = 0;
	removePlaneThings();
}

// sub $B21A: no plane (game over or attract mode).
void Game::gameOverScreen() {
	frameCount = 0xFF;
	plane.state = kNoPlane;
	attractTimer = 0xFF;
	removePlaneThings();
}

// B204-B220: the bullet and the helicopter's missile go.
void Game::removePlaneThings() {
	bullet.remove();
	world.objects.shooterSlot = kNoSlot;
}

// A5AA-A5B4: the bullet to row y.
void Game::moveBullet(int y) { bullet.moveTo(y); }

// sub $B247: two-player game: the other player's turn.
void Game::swapPlayers() {
	if (!(gameNumber & 1)) return;
	player ^= 1;
	RiverState &s = world.gen.s;
	PlayerData current;
	current.level = s.level;
	current.bridges = bridges;
	current.score = score;
	std::memcpy(current.checkpoint, s.checkpoint, 4);
	current.lives = lives;
	s.level = other.level;
	bridges = other.bridges;
	score = other.score;
	std::memcpy(s.checkpoint, other.checkpoint, 4);
	lives = other.lives;
	other = current;
}

// sub $B261: the score (this or the other player's) becomes the high score of this pair of
// games if not lower.
void Game::updateHighScore(bool otherPlayer) { highScores.update(otherPlayer ? other.score : score, gameNumber); }

// sub $AF5B: the lives as plane symbols in the panel (the newest blinks while the bonus sounds).
void Game::showLives() {
	int shown = lives;
	if (shown && (sound.bonus & 0x08)) shown--;
	panel.showLives(shown);
}

// B086-B0C6: BCD bytes as digits in the panel, leading zeros blank; nothing in the attract mode.
template <int N> void Game::printBcd(const Bcd<N> &bcd, int at, uint8_t digitZero) {
	if (!attract(*this)) panel.print(bcd, at, digitZero);
}
void Game::showBridges() { printBcd(bridges, Panel::kBridges, Panel::kDigitsBridges); }
void Game::showScore() { printBcd(score, Panel::kScore, Panel::kDigitsScore); }
void Game::showOtherScore() { printBcd(score, Panel::kHighScore, Panel::kDigitsOther); }
void Game::showHighScore() { printBcd(highScores.of(gameNumber), Panel::kHighScore, Panel::kDigitsHighScore); }

// A000-A17C: power-on.
void Game::powerOn(bool pal) {
	pal_ = pal;
	world.gen.s = RiverState();
	world.gen.s.depotLevelLimit = pal ? 0x2F : 0x2E;
	resetState();
	river.clear();
	for (int i = 0; i < kPanelBytes; i++) panel.chars[i] = rom_.panelText(i);
	// A15D (X = $09): zero page cleared, then a new game's reset
	reset(true);
	setUpGame();
	powerOnDone = 1;
	attractTimer = 1;
	plane.state = kNoPlane;
}

}  // namespace rr
