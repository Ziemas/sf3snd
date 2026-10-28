#ifndef SND_H_
#define SND_H_

#include "common.h"

#include <optional>
#include <unordered_map>
#include <vector>

struct Sound {
    u8 flags;
    std::vector<u8> track_sequence[16];
};

struct Sample {
    u32 loopAddr;
    u32 key;
    std::vector<s8> pcm;
};

struct Tone {
    u8 range;
    u8 pan;
    u8 volume;
    u8 unk;
    u16 sampleIdx;
    s8 fineTune;
    u8 attackRate;
    u8 decayRate;
    u8 sustainLevel;
    u8 sustainRate;
    u8 releaseRate;
};

struct Program {
    std::vector<Tone> instrument;
};

struct Bank {
    std::optional<Program> program[128];
};

struct SoundData {
    u8 bgmVol;
    u8 volume;

    std::unordered_map<u32, Sound> sound;
    Bank instrumentBank[16];
    Sample sample[0x300];

	u16 ar_table[64];
	u16 dr_table[64];
	u16 lfo_table[128];
	u16 vibrato_table[128];
	u16 tremolo_table[128];
	u16 freq_table[3072];
};

#endif // SND_H_
