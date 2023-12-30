#pragma once
struct ReplayInput {
	unsigned short direction : 4;  // follows 123456789 notation
	unsigned short punch : 1;  // 0x0010
	unsigned short kick : 1;  // 0x0020
	unsigned short slash : 1;  // 0x0040
	unsigned short heavySlash : 1;  // 0x0080
	unsigned short dust : 1;  // 0x0100
	unsigned short taunt : 1;  // 0x0200
	unsigned short special : 1;  // 0x0400
	unsigned short play : 1;  // 0x0800
	unsigned short record : 1;  // 0x1000
	unsigned short reset : 1;  // 0x2000
	unsigned short repeatCount : 2;  // 0x4000 (1), 0x8000 (2), 0xC000 (3)
};