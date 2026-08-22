#ifndef PLAYER_H_
#define PLAYER_H_

#include "snd_data.h"

#include <memory>

struct sndChannel {
    u8* seq_ptr;
    u8* sequence;
    Sample* sample;
    Program* prog;
    u8* loopPoint[4];

    int pitch;
    int duration; // duration of current note
    int delay; // delay until next sequence event
    int currentPitch;

    ushort vibrato;
    ushort tremolo;
    ushort lfoRate;

    u8 loopFlags[4];
	u8 note;
    u8 flags;
    u8 pitchBend;
    u8 fineTune;
    u8 bankId;
    u8 progId;
    s8 volume;
    u8 velocity;
};

struct sndVoice {
    std::vector<u8>* sample;

    u32 counter;
    u32 pos;
    u32 pitch;
    u32 voll;
    u32 volr;

    bool key;
    bool loop;
};

enum chFlag {
    SEQ_PORTAMENTO = 0x2,
    SEQ_DELAY = 0x20,
    SEQ_END = 0x40,
    SEQ_INACTIVE = 0x80,
};

class Sf3Player {
public:
    Sf3Player(std::unique_ptr<SoundData> _data)
        : data(std::move(_data)) {};

    static std::unique_ptr<Sf3Player> makePlayer(std::unique_ptr<SoundData> _data);
    void SsRequest(int sound);
    void SsRequestPan(int sound, int pan);

    void Step(int steps, s16* out);

private:
    void StepSequencer();
    void StepSequence(sndChannel& ch, int idx, bool bgm);
    void StepChannel(sndChannel& ch, int idx, bool bgm);
    void StepSynth(s16* out);

	u64 sequence_acc;

    sndChannel bgm_chan[16];
    sndChannel sfx_chan[16];

    sndVoice voice[16];

    u32 bgm_tempo;
    u32 channel_tempo[16];

    std::unique_ptr<SoundData> data;

};

#endif // PLAYER_H_
