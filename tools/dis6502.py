#!/usr/bin/env python3
"""Minimal recursive-descent 6502 disassembler for 8K Atari cartridge at $A000."""
import sys, json

BASE = 0xA000

# opcode: (mnemonic, mode)
OPS = {}
def add(m, modes):
	for mode, op in modes.items():
		OPS[op] = (m, mode)

add('ADC', dict(imm=0x69, zp=0x65, zpx=0x75, abs=0x6D, absx=0x7D, absy=0x79, indx=0x61, indy=0x71))
add('AND', dict(imm=0x29, zp=0x25, zpx=0x35, abs=0x2D, absx=0x3D, absy=0x39, indx=0x21, indy=0x31))
add('ASL', dict(acc=0x0A, zp=0x06, zpx=0x16, abs=0x0E, absx=0x1E))
add('BCC', dict(rel=0x90)); add('BCS', dict(rel=0xB0)); add('BEQ', dict(rel=0xF0))
add('BIT', dict(zp=0x24, abs=0x2C))
add('BMI', dict(rel=0x30)); add('BNE', dict(rel=0xD0)); add('BPL', dict(rel=0x10))
add('BRK', dict(imp=0x00)); add('BVC', dict(rel=0x50)); add('BVS', dict(rel=0x70))
add('CLC', dict(imp=0x18)); add('CLD', dict(imp=0xD8)); add('CLI', dict(imp=0x58)); add('CLV', dict(imp=0xB8))
add('CMP', dict(imm=0xC9, zp=0xC5, zpx=0xD5, abs=0xCD, absx=0xDD, absy=0xD9, indx=0xC1, indy=0xD1))
add('CPX', dict(imm=0xE0, zp=0xE4, abs=0xEC)); add('CPY', dict(imm=0xC0, zp=0xC4, abs=0xCC))
add('DEC', dict(zp=0xC6, zpx=0xD6, abs=0xCE, absx=0xDE))
add('DEX', dict(imp=0xCA)); add('DEY', dict(imp=0x88))
add('EOR', dict(imm=0x49, zp=0x45, zpx=0x55, abs=0x4D, absx=0x5D, absy=0x59, indx=0x41, indy=0x51))
add('INC', dict(zp=0xE6, zpx=0xF6, abs=0xEE, absx=0xFE))
add('INX', dict(imp=0xE8)); add('INY', dict(imp=0xC8))
add('JMP', dict(abs=0x4C, ind=0x6C)); add('JSR', dict(abs=0x20))
add('LDA', dict(imm=0xA9, zp=0xA5, zpx=0xB5, abs=0xAD, absx=0xBD, absy=0xB9, indx=0xA1, indy=0xB1))
add('LDX', dict(imm=0xA2, zp=0xA6, zpy=0xB6, abs=0xAE, absy=0xBE))
add('LDY', dict(imm=0xA0, zp=0xA4, zpx=0xB4, abs=0xAC, absx=0xBC))
add('LSR', dict(acc=0x4A, zp=0x46, zpx=0x56, abs=0x4E, absx=0x5E))
add('NOP', dict(imp=0xEA))
add('ORA', dict(imm=0x09, zp=0x05, zpx=0x15, abs=0x0D, absx=0x1D, absy=0x19, indx=0x01, indy=0x11))
add('PHA', dict(imp=0x48)); add('PHP', dict(imp=0x08)); add('PLA', dict(imp=0x68)); add('PLP', dict(imp=0x28))
add('ROL', dict(acc=0x2A, zp=0x26, zpx=0x36, abs=0x2E, absx=0x3E))
add('ROR', dict(acc=0x6A, zp=0x66, zpx=0x76, abs=0x6E, absx=0x7E))
add('RTI', dict(imp=0x40)); add('RTS', dict(imp=0x60))
add('SBC', dict(imm=0xE9, zp=0xE5, zpx=0xF5, abs=0xED, absx=0xFD, absy=0xF9, indx=0xE1, indy=0xF1))
add('SEC', dict(imp=0x38)); add('SED', dict(imp=0xF8)); add('SEI', dict(imp=0x78))
add('STA', dict(zp=0x85, zpx=0x95, abs=0x8D, absx=0x9D, absy=0x99, indx=0x81, indy=0x91))
add('STX', dict(zp=0x86, zpy=0x96, abs=0x8E)); add('STY', dict(zp=0x84, zpx=0x94, abs=0x8C))
add('TAX', dict(imp=0xAA)); add('TAY', dict(imp=0xA8)); add('TSX', dict(imp=0xBA))
add('TXA', dict(imp=0x8A)); add('TXS', dict(imp=0x9A)); add('TYA', dict(imp=0x98))

SIZE = dict(imp=1, acc=1, imm=2, zp=2, zpx=2, zpy=2, indx=2, indy=2, rel=2, abs=3, absx=3, absy=3, ind=3)

HW = {}
for name, a in [('HPOSP0',0xD000),('HPOSP1',0xD001),('HPOSP2',0xD002),('HPOSP3',0xD003),
				('HPOSM0',0xD004),('HPOSM1',0xD005),('HPOSM2',0xD006),('HPOSM3',0xD007),
				('SIZEP0',0xD008),('SIZEP1',0xD009),('SIZEP2',0xD00A),('SIZEP3',0xD00B),('SIZEM',0xD00C),
				('GRAFP0',0xD00D),('GRAFP1',0xD00E),('GRAFP2',0xD00F),('GRAFP3',0xD010),('GRAFM',0xD011),
				('COLPM0',0xD012),('COLPM1',0xD013),('COLPM2',0xD014),('COLPM3',0xD015),
				('COLPF0',0xD016),('COLPF1',0xD017),('COLPF2',0xD018),('COLPF3',0xD019),('COLBK',0xD01A),
				('PRIOR',0xD01B),('VDELAY',0xD01C),('GRACTL',0xD01D),('HITCLR',0xD01E),('CONSOL',0xD01F),
				('M0PF',0xD000),
				('AUDF1',0xD200),('AUDC1',0xD201),('AUDF2',0xD202),('AUDC2',0xD203),
				('AUDF3',0xD204),('AUDC3',0xD205),('AUDF4',0xD206),('AUDC4',0xD207),
				('AUDCTL',0xD208),('STIMER',0xD209),('SKRES',0xD20A),('POTGO',0xD20B),
				('SEROUT',0xD20D),('IRQEN',0xD20E),('SKCTL',0xD20F),
				('PORTA',0xD300),('PORTB',0xD301),('PACTL',0xD302),('PBCTL',0xD303),
				('DMACTL',0xD400),('CHACTL',0xD401),('DLISTL',0xD402),('DLISTH',0xD403),
				('HSCROL',0xD404),('VSCROL',0xD405),('PMBASE',0xD407),('CHBASE',0xD409),
				('WSYNC',0xD40A),('VCOUNT',0xD40B),('PENH',0xD40C),('PENV',0xD40D),
				('NMIEN',0xD40E),('NMIRES',0xD40F),
				('SETVBV',0xE45C),('SYSVBV',0xE45F),('XITVBV',0xE462)]:
	HW[a] = name
# read-side aliases at same addrs
HW_READ = {0xD000:'M0PF',0xD001:'M1PF',0xD002:'M2PF',0xD003:'M3PF',0xD004:'P0PF',0xD005:'P1PF',0xD006:'P2PF',0xD007:'P3PF',
		   0xD008:'M0PL',0xD009:'M1PL',0xD00A:'M2PL',0xD00B:'M3PL',0xD00C:'P0PL',0xD00D:'P1PL',0xD00E:'P2PL',0xD00F:'P3PL',
		   0xD010:'TRIG0',0xD011:'TRIG1',0xD012:'TRIG2',0xD013:'TRIG3',0xD014:'PAL',0xD01F:'CONSOL',
		   0xD200:'POT0',0xD201:'POT1',0xD208:'ALLPOT',0xD209:'KBCODE',0xD20A:'RANDOM',0xD20E:'IRQST',0xD20F:'SKSTAT',
		   0xD40B:'VCOUNT',0xD40F:'NMIST'}
OS = {0x0200:'VDSLST',0x0201:'VDSLST+1',0x0202:'VPRCED',0x0204:'VINTER',0x0206:'VBREAK',0x0208:'VKEYBD',
	  0x0210:'VSERIN',0x0216:'VIMIRQ',0x0222:'VVBLKI',0x0223:'VVBLKI+1',0x0224:'VVBLKD',0x0225:'VVBLKD+1',
	  0x022F:'SDMCTL',0x0230:'SDLSTL',0x0231:'SDLSTH',0x026F:'GPRIOR',
	  0x02C0:'PCOLR0',0x02C1:'PCOLR1',0x02C2:'PCOLR2',0x02C3:'PCOLR3',0x02C4:'COLOR0',0x02C5:'COLOR1',0x02C6:'COLOR2',0x02C7:'COLOR3',0x02C8:'COLOR4',
	  0x0270:'PADDL0',0x0278:'STICK0',0x0284:'STRIG0',
	  0x0012:'RTCLOK0',0x0013:'RTCLOK1',0x0014:'RTCLOK2'}

def lab(a, is_write=None):
	if a in HW or a in HW_READ:
		if is_write is False and a in HW_READ: return HW_READ[a]
		if is_write is True and a in HW: return HW[a]
		return HW.get(a, HW_READ.get(a))
	if a in OS: return OS[a]
	return None

class Dis:
	def __init__(self, data, base=BASE):
		self.d = data; self.base = base; self.end = base + len(data)
		self.insn = {}       # addr -> (len, mn, mode, operand)
		self.labels = {}     # addr -> kind  ('code','sub','data')
		self.entries = []
		self.dyn = []        # indirect jumps noted

	def inrom(self, a): return self.base <= a < self.end
	def byte(self, a): return self.d[a - self.base]

	def decode(self, a):
		op = self.byte(a)
		if op not in OPS: return None
		mn, mode = OPS[op]
		n = SIZE[mode]
		if a + n > self.end: return None
		if n == 1: val = None
		elif n == 2: val = self.byte(a+1)
		else: val = self.byte(a+1) | self.byte(a+2) << 8
		return n, mn, mode, val

	def trace(self, start, kind='code'):
		stack = [start]
		if self.inrom(start):
			self.labels.setdefault(start, kind)
		while stack:
			a = stack.pop()
			while self.inrom(a) and a not in self.insn:
				r = self.decode(a)
				if r is None: break
				n, mn, mode, val = r
				self.insn[a] = r
				if mode == 'rel':
					t = (a + 2 + (val - 256 if val > 127 else val)) & 0xFFFF
					self.labels.setdefault(t, 'code'); stack.append(t)
				elif mn == 'JSR':
					if self.inrom(val): self.labels[val] = 'sub'; stack.append(val)
				elif mn == 'JMP':
					if mode == 'abs':
						if self.inrom(val): self.labels.setdefault(val, 'code'); stack.append(val)
					else:
						self.dyn.append(a)
					break
				elif mn in ('RTS', 'RTI', 'BRK'):
					break
				a += n

	def render(self):
		out = []
		a = self.base
		while a < self.end:
			if a in self.labels:
				out.append(f"\n{'sub' if self.labels[a]=='sub' else 'L'}_{a:04X}:")
			if a in self.insn:
				n, mn, mode, val = self.insn[a]
				out.append(f"  {a:04X}  {' '.join('%02X'%self.byte(a+i) for i in range(n)):<9} {self.fmt(a, mn, mode, val)}")
				a += n
			else:
				# data run
				b = a
				run = []
				while b < self.end and b not in self.insn and (b == a or b not in self.labels) and len(run) < 16:
					run.append(self.byte(b)); b += 1
				out.append(f"  {a:04X}  .byte {' '.join('%02X'%x for x in run)}")
				a = b
		return '\n'.join(out)

	def fmt(self, a, mn, mode, val):
		w = mn in ('STA', 'STX', 'STY', 'INC', 'DEC', 'ASL', 'LSR', 'ROL', 'ROR')
		def sym(v, force_write=None):
			l = lab(v, w if force_write is None else force_write)
			return l if l else (f"${v:04X}" if v > 255 else f"${v:02X}")
		if mode == 'imp': return mn
		if mode == 'acc': return mn + ' A'
		if mode == 'imm': return f"{mn} #${val:02X}"
		if mode == 'rel':
			t = (a + 2 + (val - 256 if val > 127 else val)) & 0xFFFF
			return f"{mn} L_{t:04X}"
		if mode == 'abs':
			if mn in ('JSR', 'JMP') and self.inrom(val):
				return f"{mn} {'sub' if self.labels.get(val)=='sub' else 'L'}_{val:04X}"
			return f"{mn} {sym(val)}"
		if mode == 'absx': return f"{mn} {sym(val)},X"
		if mode == 'absy': return f"{mn} {sym(val)},Y"
		if mode == 'zp': return f"{mn} {sym(val)}"
		if mode == 'zpx': return f"{mn} {sym(val)},X"
		if mode == 'zpy': return f"{mn} {sym(val)},Y"
		if mode == 'indx': return f"{mn} (${val:02X},X)"
		if mode == 'indy': return f"{mn} (${val:02X}),Y"
		if mode == 'ind': return f"{mn} (${val:04X})"
		return mn

if __name__ == '__main__':
	rom = open(sys.argv[1], 'rb').read()
	extra = [int(x, 16) for x in sys.argv[3:]]
	d = Dis(rom)
	d.trace(0xA000, 'sub')
	for e in extra: d.trace(e, 'sub')
	open(sys.argv[2], 'w').write(d.render())
	cov = sum(v[0] for v in d.insn.values())
	print(f"code bytes: {cov} / {len(rom)}  insns: {len(d.insn)}  labels: {len(d.labels)}  indirect jmps: {[hex(x) for x in d.dyn]}")
