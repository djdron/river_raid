// River viewer (window, sound and input through SDL: sdl/host.h): the game's attract mode, the flight over the river that the original
// shows when nobody presses START.
//
// usage: river_view [rom] [-os file] [-pal] [-colours ntsc|pal] [-palette altirra|atari800] [-border] [-filter sharp|nearest|linear] [-sound] [-seed N] [-bridge N] [-random N]
//   rom: path to the cartridge image; by default rom/River_Raid.rom is looked for in the
//   current folder and up to three folders above it.
//   -os file: an Atari OS ROM (10 KB or 16 KB) or its 1 KB font, for Atari's own letters;
//   without it the AltirraOS font is used (os_font.h).
//   -seed N: another river (and objects): game 1's river starts from generator state N
//            (1..16777215); games 3-8 start at its bridges 5, 20, 50; difficulty grows as usual
//   -bridge N: fly to bridge N (1..99999) of game 1's river first, the river only (a few
//            seconds for 9999), then show it as the flight from the start would: objects as
//            they would be there, and the whole way back to the start can be looked at
//   -random N: start of POKEY's random counter (otherwise taken from the clock, as on the real
//   machine, where it depends on the moment of power-on).
//   -border: show the whole frame with its black border, not just the game's picture
//   -filter: how the picture is scaled to the window: sharp (default: whole-number scale, then
//   smoothly to fit), nearest (blocky, pixels of slightly uneven size) or linear (blurred)
//   -sound: the attract mode with sound (M switches): see the key M
//
// What it shows, as the original's attract mode does (game 1): the game running without a
// plane (game.h): the river scrolls down 1 line per frame (60 frames a second), ships and
// helicopters wake up and move, tanks drive to the bridges and fire, helicopters fire
// missiles; below, the status panel and the scrolling line with the credits, "Press START key"
// and the rainbow Activision logo. It starts on an empty screen, like after power-on.
//
// Keys:
//   Space          pause / continue
//   Up / Down      faster / slower, through a stop to flying backwards (x1/4 .. x16 either way)
//   mouse wheel    scroll along the river by hand (PgUp / PgDn by a screen)
//   Backspace      back to the newest line, normal speed
//   B / Shift+B    next / previous bridge; with Alt 10 bridges, with Ctrl 100
//   Home           to bridge 1, the start of the river (as Shift+B back there)
//   F1 / F2 / F3   sound test: explosion / firing / score sound, through the game's own sound
//                  driver (the attract mode itself is silent, as in the original)
//   F              full screen
//   Esc            leave full screen
//   C / V          NTSC / PAL colours; Altirra / atari800 colour model
//   O              the game's picture only / the whole frame with its border
//   L              scaling: sharp / nearest / linear
//   M              sound on / off: tank shells, helicopter missiles and a plane's engine (not in
//                  the original, whose attract mode is silent; -sound starts with it on); flying
//                  forward at any speed, the engine following it; backwards, played backwards
// A mouse click or going full screen hides the mouse pointer until the mouse moves. Closing the window (or Alt+F4,
// Cmd+Q) quits.
//
// Looking back runs the attract mode again from a snapshot up to the moment asked for, so the
// past is shown as it was, objects moving, flying forwards or backwards; the sound plays only
// in the present.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <iterator>
#include <map>
#include <new>
#include <optional>
#include <string>
#include <vector>

#include "game.h"
#include "os_font.h"
#include "pokey_random.h"
#include "pokey_sound.h"
#include "river_draw.h"
#include "river_gen.h"
#include "status_bar.h"

#include "host.h"
#include "host_main.h"

namespace {

// Part of the 384 x 240 screen buffer shown in the window: everything that is ever drawn.
constexpr int kViewLeft = 10, kViewWidth = 364;
constexpr int kViewTop = 0, kViewHeight = rr::kScreenHeight;
constexpr int kScale = 6;   // initial window scale (less if the screen is smaller)
constexpr int kSampleRate = 48000;

// The attract mode: the game running without a player, from power-on, plus the status bar.
//
// The present is run once (head_); the sections of the river it generates (bridge to bridge)
// are recorded. Looking back runs the past again: a second copy (replay_) is started from a
// saved state and run to the line asked for, so any earlier moment is shown as it was, objects
// moving.
//
// Jumping: the river does not depend on the objects, and the objects live only while they are
// on screen, so the state at any moment follows from the river alone plus the last
// kSettleFrames frames: the machine runs ahead with the river only (Machine::skip, the objects
// standing still), then kSettleFrames whole frames, by which time every object from before
// has scrolled off and the ones on screen have lived their whole lives as they did. (Checked
// against flying all the way: every following frame drawn the same and the whole state the
// same, at hundreds of points of several rivers, from settle times of 200 frames on, about the
// time an object takes to cross the screen; kSettleFrames has twice that.)
// So B flies ahead by the river only, and far back the replay starts from a small anchor
// (the river's state, about 170 bytes, every kAnchorFrames frames). Whole snapshots (about
// 16 KB) are kept only near the present and around where the replay is, for running back
// smoothly.
class Attract {
public:
	Attract(const rr::Rom &rom, const uint8_t osFont[rr::kOsFontBytes], bool pal, uint32_t seed)
		: rom_(rom), pal_(pal), seed_(seed), charsets_(rr::buildCharsets(rom, osFont)), head_(rom, seed, pal) {
		rr::initialPanel(rom, panel_);
		restart();
	}

	void restart() {
		head_ = Machine(rom_, seed_, pal_);
		head_.game.gen.setRiver(riverSeed_, 0);
		sections_.clear();
		anchors_.clear();
		snapshots_.clear();
		cachedCount_ = 0;
		replay_.reset();
		std::memset(live_, 0, sizeof live_);
		liveStale_ = false;
		pokey_ = rr::PokeySound(kSampleRate, rr::tvSystem(pal_).clock);
		audio_.clear();
		if (startBridge_ > 1) flyToBridge(startBridge_);
	}

	// The line where a bridge is shown on a jump (B, -bridge): a little above the bottom of the
	// river area.
	static constexpr long kBridgeMargin = 100;

	// Flies the present from where it is to bridge `n` (its line kBridgeMargin lines down), the
	// river only until close to it.
	void flyToBridge(int n) {
		for (long stalled = 0; sections_.empty() || sections_.back().bridge < n - 1; ) {
			const long before = lines();
			keep();
			liveStale_ = true;
			head_.skip(pal_, newLine());
			stalled = lines() == before && head_.running ? stalled + 1 : 0;
			if (stalled >= 4096) return;
		}
		if (sections_.back().bridge >= n) return;
		const long next = nextBridge(lines() - 1);
		if (next >= 0) flyTo(next + kBridgeMargin);
	}

	// One frame of the original, in the present. fast: skip what is only seen or heard (the
	// picture and the sound), for flying ahead. sound: play this frame's sound (at normal speed).
	void frame(bool fast = false, bool sound = true) {
		keep();
		liveStale_ = fast;
		head_.game.attractSound = sounds_;
		head_.game.attractEnginePitch = engine_.first;
		head_.game.attractEngineStick = engine_.second;
		head_.step(pal_, fast ? nullptr : live_, newLine());
		play(head_, !fast && sound);
	}

	// The attract mode with sound (-sound, M; not in the original, see Game::attractSound): the
	// tank shells, the helicopters' missiles and the engine of a plane flying.
	void setSounds(bool on) { sounds_ = on; }
	bool sounds() const { return sounds_; }
	// The engine follows the viewer's speed (speed: frames of the original per real frame), as a
	// plane speeding up or slowing down would: up to x2 as in a game (stick up, $FE at x2; stick
	// down, $40 at x1/4), higher above it.
	void setEngine(double speed) {
		uint8_t pitch = 0, stick = 0x0F;
		if (speed > 1) {
			stick = 0x0E;   // up
			pitch = speed <= 2 ? 0x10 : speed <= 4 ? 0x0C : speed <= 8 ? 0x09 : 0x07;
		} else if (speed < 1) {
			stick = 0x0D;   // down
			pitch = speed <= 0.25 ? 0x1B : 0x19;
		}
		engine_ = {pitch, stick};
	}
	// Sound for `seconds` of real time from the registers as they are (at speeds other than x1,
	// where frames do not come one per frame time of the Atari).
	void playFor(double seconds) {
		if (sounds_) pokey_.run(long(seconds * pokey_.clock()), audio_);
	}

	// Flies the present on until line `line` has come (the river only while it is far, whole
	// frames for the last kSettleFrames, drawing only the last one); false if the river stops
	// coming.
	bool flyTo(long line) {
		for (long stalled = 0; lines() - 1 < line; ) {
			const long before = lines();
			if (line - (lines() - 1) > kSettleFrames) {
				keep();
				liveStale_ = true;
				head_.skip(pal_, newLine());
			} else {
				frame(lines() < line);
			}
			stalled = lines() == before ? stalled + 1 : 0;
			if (stalled >= 4096) {
				frame();
				return false;
			}
		}
		return true;
	}

	// First line of the section of the next bridge after `line`, generating ahead with the
	// river alone if needed (-1 if the river stops coming).
	long nextBridge(long line) {
		const long known = bridgeAfter(line);
		if (known >= 0) return known;
		rr::RiverGen gen = head_.game.gen;
		const uint8_t level = sections_.empty() ? gen.s.level : sections_.back().level;
		uint8_t buf[rr::kLineBytes];
		for (long n = lines(); n < lines() + 100000; n++) {
			gen.nextLine(buf);
			if (gen.s.level != level && n > line) return n;
		}
		return -1;
	}

	// Sound test: start one of the game's sound effects through its own sound driver.
	// (The attract mode itself is silent, as in the original.)
	void soundTest(int which) {
		if (!head_.running) return;
		switch (which) {
		case 0: head_.game.g.soundExplosion = 0x17; break;   // explosion, as A4E6 sets it
		case 1: head_.game.g.soundFire = 0x0F; break;        // firing (A5A6)
		case 2: head_.game.g.soundBonus = 0x40; break;       // score (A587)
		default: break;
		}
	}
	// Samples generated so far (taken by the caller).
	std::vector<int16_t> &audio() { return audio_; }

	long lines() const { return head_.lines; }
	// First line of the bridge section before / after `line` (before: 0 if none; after: -1 if
	// not generated yet).
	long bridgeBefore(long line) const {
		long found = 0;
		for (size_t i = 1; i < sections_.size() && sections_[i].first < line; i++) found = sections_[i].first;
		return found;
	}
	long bridgeAfter(long line) const {
		const size_t i = sectionOf(line) + 1;
		return i < sections_.size() && i > 0 ? sections_[i].first : -1;
	}
	// First line of the section of bridge `number` (0 for the start or a bridge before it, -1 if
	// not generated yet).
	long bridgeLine(int number) const {
		if (sections_.empty()) return -1;
		const long i = long(number) - sections_[0].bridge;
		if (i <= 0) return 0;
		return i < long(sections_.size()) ? sections_[size_t(i)].first : -1;
	}
	int level(long line) const { return line >= 0 && line < lines() ? sections_[sectionOf(line)].level : 0; }
	int bridge(long line) const { return line >= 0 && line < lines() ? sections_[sectionOf(line)].bridge : 0; }

	// Draws the screen at the moment line `back` lines before the newest one entered it: the
	// present for back = 0, otherwise that moment run again. The panel's BRIDGE counter shows
	// the bridge of the newest line on screen.
	// sound: when the replay has moved on by one frame (looking back, flying forward at normal
	// speed), play that frame's sound.
	void draw(long back, uint8_t screen[rr::kScreenHeight][rr::kScreenWidth], bool sound = false) {
		const Machine *m = &head_;
		const uint8_t (*live)[rr::kRowWidth] = live_;
		const long target = lines() - 1 - back;
		// (the present not drawn, or not settled after a jump: run it again too)
		if ((back > 0 || liveStale_ || !head_.exact()) && target >= 0) {
			const long before = replay_ ? replay_->frame : -1;
			replayTo(target);
			if (back > 0) play(*replay_, sound && replay_->frame == before + 1);
			m = &*replay_;
			live = replayLive_;
		}
		std::memset(screen, 0, size_t(rr::kScreenHeight) * rr::kScreenWidth);
		std::memcpy(screen[rr::kRiverTop], live[rr::kRiverTop], size_t(rr::kRiverRows) * rr::kRowWidth);
		uint8_t panel[rr::kPanelBytes];
		std::memcpy(panel, panel_, sizeof panel);
		if (lines() > 0) rr::showPanelBridge(panel, bridge(std::max(target, 0L)));
		rr::drawStatusBar(rom_, charsets_, panel, 0xFF, m->scroller, m->logoShown, screen, pal_);   // full tank
	}

	// Gets the moment `back` lines before the newest one ready for draw(), for a jump (B /
	// Shift+B): without the stretch kept for running back from it, so the jump is as quick as
	// flying ahead; running back from there gets it when it is needed.
	void jumpTo(long back) {
		const long target = lines() - 1 - back;
		if (back > 0 && target >= 0) replayTo(target, 0);
	}

	void setRiver(uint32_t seed, int bridge) {
		riverSeed_ = seed;
		startBridge_ = bridge;
		restart();
	}

private:
	static constexpr long kSnapshotFrames = 128;     // snapshot spacing near the present and the replay
	static constexpr long kRecentFrames = 8192;      // the stretch before the present kept so
	static constexpr size_t kReplaySnapshots = 64;   // snapshots kept around the replay
	static constexpr long kSettleFrames = 400;       // whole frames after running on with the river only
	static constexpr long kAnchorFrames = 1024;      // anchor spacing
	static constexpr long kCoverFrames = 512;        // whole frames before a far line, for running back

	// Everything that runs: the game, POKEY's random counter, the scrolling line. Copied whole
	// for a snapshot.
	struct Machine {
		rr::Game game;
		rr::PokeyRandom random;
		rr::Scroller scroller;
		uint8_t logoShown = 0;
		long frame = 0;
		long lines = 0;             // lines generated so far
		bool running = false;
		long exactFrom = 0;         // the objects are right from this frame on (after a skip)

		Machine(const rr::Rom &rom, uint32_t seed, bool pal) : game(rom), random(seed, rr::tvSystem(pal)) {}
		Machine(const Machine &) = default;
		Machine &operator=(Machine &&m) {   // Game holds a reference to the ROM: rebuild in place
			this->~Machine();
			new (this) Machine(m);
			return *this;
		}
		bool exact() const { return frame >= exactFrom; }

		// One frame of the original. Timing after power-on as in the original: the scrolling
		// line starts moving at frame 11, the game (and its river) at frame 38 (9 and 25 on PAL,
		// Game::attractStartFrame). live: if not null, the
		// river area is drawn there. newLine: called with the generator state after each new line.
		void step(bool pal, uint8_t (*live)[rr::kRowWidth], const std::function<void(const rr::RiverState &)> &newLine) {
			frame++;
			game.afterLine = [this, &newLine](const uint8_t *) {
				lines++;
				if (newLine) newLine(game.gen.s);
			};
			if (frame == rr::Game::attractStartFrame(pal)) {
				game.startAttract(pal);
				running = true;
			} else if (running) {
				game.prepare();
				game.display();
				if (live) game.drawRiver(live);
				// The original turns the screen off and stops after 128 x 2048 frames (about 73 minutes)
				// without input (A1F5, $78): the viewer keeps flying, as if a key had just been touched.
				game.g.idleTime = 0;
				game.logic(rr::Input(), [this, pal] {   // A205, after scanline 160
					return pal ? random.read(191, 24) : random.read(192, 13);
				});
				game.vbi([this] { return random.read(248, 30); });
			}
			game.afterLine = nullptr;
			random.nextFrame();
			logoShown = scroller.logoX;   // the display uses the position from before the step
			if (frame >= rr::Game::scrollerStartFrame(pal)) scroller.step(true);
		}
		// The same frame for the river only (Game::skipFrame): the objects stand still, so they
		// are right again kSettleFrames whole frames later.
		void skip(bool pal, const std::function<void(const rr::RiverState &)> &newLine) {
			if (!running || frame + 1 == rr::Game::attractStartFrame(pal)) {
				step(pal, nullptr, newLine);
				return;
			}
			frame++;
			exactFrom = frame + kSettleFrames;
			game.afterLine = [this, &newLine](const uint8_t *) {
				lines++;
				if (newLine) newLine(game.gen.s);
			};
			game.g.idleTime = 0;
			game.skipFrame();
			game.afterLine = nullptr;
			random.nextFrame();
			logoShown = scroller.logoX;
			scroller.step(true);
		}
	};

	// What Machine::skip changes: the river and the counters. A machine set to it runs on
	// exactly, but its objects are those of another moment until it has settled.
	struct Anchor {
		rr::RiverState river;
		uint8_t frameCount, attractFlags, attractTimer, scrollFrac, gameLogo;
		rr::Scroller gameScroller;
		uint16_t screenTop;
		rr::PokeyRandom random;
		rr::Scroller scroller;
		uint8_t logoShown;
		long frame, lines;
		explicit Anchor(const Machine &m)
			: river(m.game.gen.s), frameCount(m.game.g.frameCount), attractFlags(m.game.g.attractFlags),
			  attractTimer(m.game.g.attractTimer), scrollFrac(m.game.g.scrollFrac), gameLogo(m.game.g.logoShown),
			  gameScroller(m.game.g.scroller), screenTop(m.game.g.screenTop), random(m.random), scroller(m.scroller),
			  logoShown(m.logoShown), frame(m.frame), lines(m.lines) {}
		void setUp(Machine &m) const {
			m.game.gen.s = river;
			m.game.g.frameCount = frameCount;
			m.game.g.attractFlags = attractFlags;
			m.game.g.attractTimer = attractTimer;
			m.game.g.scrollFrac = scrollFrac;
			m.game.g.logoShown = gameLogo;
			m.game.g.scroller = gameScroller;
			m.game.g.screenTop = screenTop;
			m.random = random;
			m.scroller = scroller;
			m.logoShown = logoShown;
			m.frame = frame;
			m.lines = lines;
			m.exactFrom = frame + kSettleFrames;
		}
	};

	struct Section {
		long first;      // its first line
		uint8_t level;   // the generator's $60
		int bridge;      // the number of the bridge it starts with, as the panel counts it
	};
	size_t sectionOf(long line) const {
		const auto i = std::upper_bound(sections_.begin(), sections_.end(), line,
			[](long l, const Section &s) { return l < s.first; });
		return i == sections_.begin() ? 0 : size_t(i - sections_.begin() - 1);
	}
	// Records each new line's section: a new level starts with each bridge.
	std::function<void(const rr::RiverState &)> newLine() {
		return [this](const rr::RiverState &s) {
			const long line = head_.lines - 1;
			if (sections_.empty()) sections_.push_back({line, s.level, head_.game.gen.start(0).bridge});
			else if (s.level != sections_.back().level) sections_.push_back({line, s.level, sections_.back().bridge + 1});
		};
	}

	// Before each frame of the present: an anchor every kAnchorFrames frames, a snapshot every
	// kSnapshotFrames frames while settled (the one leaving the recent stretch goes).
	void keep() {
		if (head_.frame % kAnchorFrames == 0 && (anchors_.empty() || anchors_.back().frame < head_.frame))
			anchors_.emplace_back(head_);
		if (head_.frame % kSnapshotFrames == 0 && head_.exact() && !snapshots_.count(head_.frame))   // (frame 0: always kept)
			snapshots_.emplace(head_.frame, Snapshot{head_, false});
		const auto old = snapshots_.find(head_.frame - kRecentFrames);
		if (old != snapshots_.end() && !old->second.cached && old->first != 0) snapshots_.erase(old);
	}

	// Sound: the registers the VBI set at the end of the frame before play for one frame.
	void play(const Machine &m, bool sound) {
		if (sound) pokey_.run(rr::tvSystem(pal_).cyclesPerFrame(), audio_);
		for (int c = 0; c < 4; c++) {
			pokey_.write(2 * c, m.game.g.pokey.audf[c]);
			pokey_.write(2 * c + 1, m.game.g.pokey.audc[c]);
		}
	}

	// Brings the replay to the moment line `target` entered the screen, from the nearest saved
	// state: where the replay is, a snapshot, or (far away) an anchor.
	void replayTo(long target, long cover = kCoverFrames) {
		auto it = snapshots_.end();   // the nearest snapshot before the line
		while (it != snapshots_.begin() && std::prev(it)->second.machine.lines - 1 >= target) --it;
		const Snapshot *snap = it == snapshots_.begin() ? nullptr : &std::prev(it)->second;
		const Anchor *anchor = nullptr;   // the nearest anchor far enough before to settle
		for (auto a = anchors_.rbegin(); a != anchors_.rend(); ++a)
			if (a->lines - 1 < target - kSettleFrames - cover && a->frame >= rr::Game::attractStartFrame(pal_)) {
				anchor = &*a;
				break;
			}
		// run on from where the replay is, unless a snapshot or an anchor is nearer
		const bool replayUsable = replay_ && replay_->lines - 1 <= target && (!snap || replay_->frame >= snap->machine.frame) &&
			(!anchor || replay_->frame >= anchor->frame);
		if (!replayUsable) {
			if (snap && (!anchor || target - snap->machine.lines <= kSettleFrames + cover || anchor->frame <= snap->machine.frame)) {
				replay_.emplace(snap->machine);
			} else if (anchor) {
				replay_.emplace(head_);   // any machine in the attract mode: its objects settle
				anchor->setUp(*replay_);
			} else {
				replay_.emplace(snapshots_.begin()->second.machine);
			}
		}
		replay_->game.attractSound = sounds_;
		replay_->game.attractEnginePitch = engine_.first;
		replay_->game.attractEngineStick = engine_.second;
		while (replay_->lines - 1 < target) {
			const long before = replay_->lines;
			// the river only while far; settled `cover` frames before the line, so snapshots are kept
			// behind it too and running back from there is smooth
			if (target - (replay_->lines - 1) > kSettleFrames + cover) {
				replay_->skip(pal_, nullptr);
			} else {   // draw only the frames that may reach the line
				replay_->step(pal_, replay_->lines >= target ? replayLive_ : nullptr, nullptr);
				if (replay_->frame % kSnapshotFrames == 0 && replay_->exact() && !snapshots_.count(replay_->frame)) keepReplay();
			}
			if (replay_->lines == before && replay_->frame > head_.frame + 4096) break;   // the river stopped
		}
	}

	const rr::Rom &rom_;
	bool pal_;
	uint32_t seed_;
	uint32_t riverSeed_ = 0;   // -seed, -bridge
	int startBridge_ = 0;      // -bridge: flown to on a (re)start
	rr::Charsets charsets_;
	uint8_t panel_[rr::kPanelBytes];
	Machine head_;                       // the present
	struct Snapshot {
		Machine machine;
		bool cached;   // made by the replay
	};
	std::map<long, Snapshot> snapshots_;   // by frame
	size_t cachedCount_ = 0;
	std::vector<Anchor> anchors_;          // every kAnchorFrames frames of the present
	std::vector<Section> sections_;

	// Keeps the replay's state as a snapshot, so running back through this stretch is quick;
	// of those, the ones farthest from the replay go first.
	void keepReplay() {
		snapshots_.emplace(replay_->frame, Snapshot{*replay_, true});
		if (++cachedCount_ <= kReplaySnapshots) return;
		auto farthest = snapshots_.end();
		long distance = -1;
		for (auto i = snapshots_.begin(); i != snapshots_.end(); ++i) {
			if (!i->second.cached) continue;
			const long d = std::labs(i->first - replay_->frame);
			if (d > distance) distance = d, farthest = i;
		}
		const bool keep = head_.frame - farthest->first < kRecentFrames;
		if (keep) farthest->second.cached = false;   // it belongs anyway
		else snapshots_.erase(farthest);
		cachedCount_--;
	}
	std::optional<Machine> replay_;      // the past, run again from a snapshot or an anchor
	uint8_t live_[rr::kScreenHeight][rr::kRowWidth] = {};        // the present's river area
	uint8_t replayLive_[rr::kScreenHeight][rr::kRowWidth] = {};  // the replay's
	rr::PokeySound pokey_{kSampleRate};
	std::vector<int16_t> audio_;
	bool liveStale_ = false;   // the present's last frame ran without drawing
	bool sounds_ = false;      // -sound, M
	std::pair<uint8_t, uint8_t> engine_{0, 0x0F};   // see setEngine
};

std::string findRom(int argc, char **argv) {
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (!std::strcmp(argv[i], "-os") || !std::strcmp(argv[i], "-shot") || !std::strcmp(argv[i], "-seed") ||
				!std::strcmp(argv[i], "-bridge") || !std::strcmp(argv[i], "-random") ||
				!std::strcmp(argv[i], "-colours") || !std::strcmp(argv[i], "-palette") ||
				!std::strcmp(argv[i], "-filter"))
				i++;
			continue;
		}
		return argv[i];
	}
	std::string up;
	for (int d = 0; d < 4; d++, up += "../") {
		const std::string p = up + "rom/River_Raid.rom";
		if (FILE *f = std::fopen(p.c_str(), "rb")) { std::fclose(f); return p; }
	}
	return "rom/River_Raid.rom";
}

void fail(const std::string &msg) { host::Host::showError("River Raid viewer", msg); }

}  // namespace

int main(int argc, char **argv) {
	bool pal = false;
	host::Filter filter = host::Filter::Sharp;   // -filter (L switches)
	bool sound = false;        // -sound: the attract mode with sound (M switches)
	bool border = false;       // -border: the whole frame with its black border (O switches)
	bool palColours = true;    // colours of a PAL machine (C switches to NTSC)
	rr::PaletteModel paletteModel = rr::PaletteModel::Altirra;   // V switches
	const char *osPath = nullptr;
	const char *shotPath = nullptr;   // testing: -shot file.bmp:N saves frame N as BMP (.raw: screen buffer) and quits
	long shotFrame = 0;
	uint32_t seed = uint32_t(host::Host::ticksNs() ^ uint64_t(std::time(nullptr)));   // like POKEY at power-on
	uint32_t riverSeed = 0;            // -seed: 0 = the cartridge's river
	int startBridge = 0;               // -bridge: 0 = the cartridge's starting bridges
	for (int i = 1; i < argc; i++) {
		if (!std::strcmp(argv[i], "-pal")) pal = palColours = true;
		else if (!std::strcmp(argv[i], "-border")) border = true;
		else if (!std::strcmp(argv[i], "-sound")) sound = true;
		else if (!std::strcmp(argv[i], "-filter") && i + 1 < argc) {
			const char *name = argv[++i];
			filter = !std::strcmp(name, "nearest") ? host::Filter::Nearest
				: !std::strcmp(name, "linear") ? host::Filter::Linear : host::Filter::Sharp;
		}
		else if (!std::strcmp(argv[i], "-colours") && i + 1 < argc) palColours = !std::strcmp(argv[++i], "pal");
		else if (!std::strcmp(argv[i], "-palette") && i + 1 < argc)
			paletteModel = !std::strcmp(argv[++i], "atari800") ? rr::PaletteModel::Atari800 : rr::PaletteModel::Altirra;
		else if (!std::strcmp(argv[i], "-os") && i + 1 < argc) osPath = argv[++i];
		else if (!std::strcmp(argv[i], "-random") && i + 1 < argc) seed = uint32_t(std::strtoul(argv[++i], nullptr, 0));
		else if (!std::strcmp(argv[i], "-seed") && i + 1 < argc) riverSeed = uint32_t(std::strtoul(argv[++i], nullptr, 0)) & 0xFFFFFF;
		else if (!std::strcmp(argv[i], "-bridge") && i + 1 < argc) startBridge = std::atoi(argv[++i]);
		else if (!std::strcmp(argv[i], "-shot") && i + 1 < argc) {
			shotPath = argv[++i];
			if (const char *colon = std::strrchr(shotPath, ':')) {
				shotFrame = std::atol(colon + 1);
				static std::string path;
				path.assign(shotPath, colon);
				shotPath = path.c_str();
			}
		}
	}

	const std::string romPath = findRom(argc, argv);
	rr::Rom rom;
	if (!rom.load(romPath)) {
		fail("Cannot read the ROM: " + romPath + "\nPass the path to the River Raid cartridge image as the first argument.");
		return 1;
	}
	uint8_t osFont[rr::kOsFontBytes];
	if (osPath) {
		if (!rr::loadOsFont(osPath, osFont)) {
			fail(std::string("Cannot read the OS font from ") + osPath + " (expected a 10 KB or 16 KB OS ROM or a 1 KB font)");
			return 1;
		}
	} else {
		rr::builtinOsFont(osFont);
	}

	const rr::TvSystem &tv = rr::tvSystem(pal);
	host::Host window;
	// the game's picture only, or the whole frame
	auto showBorder = [&]() {
		if (border) window.setView(0, 0, kViewWidth, kViewHeight);
		else window.setView(rr::kPictureLeft - kViewLeft, rr::kPictureTop - kViewTop, rr::kPictureWidth, rr::pictureHeight(pal));
	};
	showBorder();   // before open(): the window is made in the size of the part shown
	if (!window.open("River Raid", kViewWidth, kViewHeight, tv.pixelAspect(), kScale, shotPath ? 0 : kSampleRate)) {
		fail(window.error());
		return 1;
	}

	uint32_t palette[256];
	auto makePalette = [&]() {
		for (int c = 0; c < 256; c++) {
			uint8_t rgb[3];
			rr::atariRgb(uint8_t(c), rgb, palColours, paletteModel);
			palette[c] = (uint32_t(rgb[0]) << 16) | (uint32_t(rgb[1]) << 8) | rgb[2];
		}
	};
	makePalette();
	// the window title shows where the river is, then the machine, the colours and the scaling in use
	std::string colours;
	auto showColours = [&]() {
		colours = std::string(palColours ? "PAL" : "NTSC") + " colours, " + rr::paletteName(paletteModel) + ", " +
			host::filterName(filter) + " scaling";
	};
	window.setFilter(filter);
	showColours();

	Attract attract(rom, osFont, pal, seed);
	attract.setSounds(sound);
	attract.setRiver(riverSeed, startBridge);
	static uint8_t screen[rr::kScreenHeight][rr::kScreenWidth];
	// Speed in original frames per real frame; negative = backwards.
	static const double kSpeeds[] = {-16, -8, -4, -2, -1, -0.5, -0.25, 0, 0.25, 0.5, 1, 2, 4, 8, 16};
	const int kSpeedCount = int(sizeof kSpeeds / sizeof kSpeeds[0]);
	int speedIndex = 10;   // x1
	double pending = 0;    // original frames still to run
	bool paused = false;
	long back = 0;         // look-back in lines
	uint64_t last = host::Host::ticksNs();
	std::string title;

	if (shotPath)
		for (long f = 0; f < shotFrame; f++) attract.frame();

	for (bool running = true; running;) {
		host::Event e;
		while (window.poll(e)) {
			using host::Key;
			if (e.type == host::Event::Type::Quit) running = false;
			else if (e.type == host::Event::Type::MouseWheel) back += long(e.wheel * 16);
			else if (e.type == host::Event::Type::KeyDown) {
				switch (e.key) {
				case Key::Space: paused = !paused; break;
				case Key::L: filter = host::Filter((int(filter) + 1) % 3); window.setFilter(filter); showColours(); break;
				case Key::O: border = !border; showBorder(); break;
				case Key::M: attract.setSounds(!attract.sounds()); break;
				case Key::C: palColours = !palColours; makePalette(); showColours(); break;
				case Key::V:
					paletteModel = paletteModel == rr::PaletteModel::Altirra ? rr::PaletteModel::Atari800 : rr::PaletteModel::Altirra;
					makePalette();
					showColours();
					break;
				case Key::Up: speedIndex = std::min(speedIndex + 1, kSpeedCount - 1); break;
				case Key::Down: speedIndex = std::max(speedIndex - 1, 0); break;
				case Key::PageUp: back += rr::kRiverRows; break;
				case Key::PageDown: back -= rr::kRiverRows; break;
				case Key::Backspace: back = 0; speedIndex = 10; break;
				case Key::Home:   // as Shift+B back to bridge 1
					back = attract.lines() - 1 - (attract.bridgeLine(0) + Attract::kBridgeMargin);
					attract.jumpTo(back);
					break;
				case Key::B: {
					// B / Shift+B: next / previous bridge; with Alt 10 bridges, with Ctrl 100. The bridge
					// line is shown a little above the bottom of the river area; Shift+B just after a
					// bridge goes to the one before it.
					constexpr long kMargin = Attract::kBridgeMargin;
					const int count = e.ctrl ? 100 : e.alt ? 10 : 1;
					const long shown = attract.lines() - 1 - back - kMargin;
					if (e.shift) {
						const int number = attract.bridge(std::max(shown - 64, 0L)) - (count - 1);
						back = attract.lines() - 1 - (attract.bridgeLine(number) + kMargin);
						attract.jumpTo(back);
					} else {
						const int number = attract.bridge(std::max(shown, 0L)) + count;
						if (attract.bridgeLine(number) < 0) attract.flyToBridge(number);   // not generated yet
						const long line = attract.bridgeLine(number);
						if (line < 0) break;
						if (attract.lines() - 1 < line + kMargin) attract.flyTo(line + kMargin);
						back = attract.lines() - 1 - (line + kMargin);
					}
					break;
				}
				case Key::F1: attract.soundTest(0); break;
				case Key::F2: attract.soundTest(1); break;
				case Key::F3: attract.soundTest(2); break;
				case Key::F: window.setFullscreen(!window.fullscreen()); break;
				case Key::Escape: if (window.fullscreen()) window.setFullscreen(false); break;
				default: break;
				}
			}
		}

		const uint64_t now = host::Host::ticksNs();
		const double dt = std::min(double(now - last) / 1e9, 0.1);
		last = now;
		if (!paused && !shotPath) {
			pending += kSpeeds[speedIndex] * dt * tv.frameRate();
			// sound: at x1 frame by frame; faster or slower, for the real time from the registers of
			// the frame shown (the engine follows the speed). Backwards the same, after draw(),
			// which sets the registers of the frame shown: the frames come in reverse order, so it
			// sounds played backwards (a whistle rises, an explosion swells). The samples are not
			// turned round: within a frame the registers hold still, so that would add nothing but a
			// jump at every frame's edge, 60 a second, which rasps.
			const bool normalSpeed = kSpeeds[speedIndex] == 1;
			attract.setEngine(std::fabs(kSpeeds[speedIndex]));
			if (kSpeeds[speedIndex] > 0 && !normalSpeed) attract.playFor(dt);
			for (; pending >= 1; pending--) {
				if (back > 0) back--;   // replay lines already generated
				else attract.frame(false, normalSpeed);
			}
			for (; pending <= -1; pending++) back++;
		}
		back = std::clamp(back, 0L, std::max(0L, attract.lines() - 1));
		{
			// at higher speeds or after a stall, playSound drops what is queued
			std::vector<int16_t> &samples = attract.audio();
			window.playSound(samples.data(), samples.size());
			samples.clear();
		}
		if (attract.lines() > 0 && back == attract.lines() - 1 && kSpeeds[speedIndex] < 0) pending = 0;

		attract.draw(back, screen, !paused && !shotPath && kSpeeds[speedIndex] == 1);
		if (!paused && !shotPath && kSpeeds[speedIndex] < 0 && back < attract.lines() - 1) attract.playFor(dt);
		uint32_t *pixels = window.pixels();
		for (int y = 0; y < kViewHeight; y++)
			for (int x = 0; x < kViewWidth; x++) pixels[y * kViewWidth + x] = palette[screen[kViewTop + y][kViewLeft + x]];

		if (shotPath && std::strstr(shotPath, ".raw")) {   // screen buffer: 240 rows of 384 colour codes
			if (FILE *f = std::fopen(shotPath, "wb")) {
				std::fwrite(screen, 1, sizeof screen, f);
				std::fclose(f);
			}
		} else if (shotPath) {
			for (int i = 0; i < 3; i++) {             // let a resized window settle
				for (host::Event e; window.poll(e);) {}
				window.present();
			}
			window.savePicture(shotPath);
		}
		if (shotPath) running = false;
		window.present();

		char buf[256];
		const long shown = attract.lines() - 1 - back;
		// the newest bridge at the top of the screen or below it: the one on screen, or the one
		// last passed (sections are much longer than the screen, so at most one is on it)
		std::snprintf(buf, sizeof buf, "River Raid - bridge %d (level %d) - line %ld - speed x%g%s%s%s - %s, %s",
			attract.bridge(shown), attract.level(shown), shown, kSpeeds[speedIndex], back ? " - looking back" : "", paused ? " - paused" : "", attract.sounds() ? " - sound" : "",
			pal ? "PAL machine (50 Hz)" : "NTSC machine (60 Hz)", colours.c_str());
		if (title != buf) {
			title = buf;
			window.setTitle(buf);
		}
	}

	return 0;
}
