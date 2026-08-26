#ifndef PLAYER_H_
#define PLAYER_H_

#include "snd_data.h"

#include <memory>

struct sndChannel {
    u8* seq_ptr;
    u8* sequence;
    Tone* tone;
    Sample* sample;

    int pitch;
    int currentPitch;
    int duration; // duration of current note
    int delay; // delay until next sequence event

    u8* loopPoint[4];

    ushort vibrato;
    ushort tremolo;
    ushort lfoRate;
    ushort unk58;
    short transpose;
    ushort portamento_unk44;
    ushort portamento_unk46;
    ushort attackStep;
    ushort attackTarget;
    ushort decayStep;
    ushort sustainStep;
    ushort sustainTarget;
    ushort releaseStep;

    // changed to int to simplify envelope checking
    int envLevel;

    u8 envState;
    u8 newNote;
    u8 noteActive;
    u8 unk64;
    u8 unk65;
    u8 unk66;
    u8 unk68;
    u8 unk6a;
    s8 loopCount[4];
    u8 note;
    u8 chFlags;
    s8 pitchBend;
    s8 fineTune;
    u8 bankId;
    u8 progId;
    s8 volume;
    u8 velocity;
    u8 pan;
    u8 expression;
    u8 seqFlags;
};

struct sndVoice {
    std::vector<s8>* sample;

    s32 counter;
    u32 loopAddr;
    u32 pos;
    s32 pitch;
    s32 voll;
    s32 volr;

    bool key;
    bool loop;

    void keyOn()
    {
        counter = 0;
        pos = 0;
        key = 1;
    }

    void keyOff()
    {
        key = 0;
    }
};

struct sndPanState {
    u16 start;
    s16 end;
    s16 step;
    s16 mode;
};

enum chFlag {
    CH_LFO = 0x1,
    CH_PORTAMENTO = 0x2,
    CH_DELAY = 0x20,
    CH_END = 0x40,
    CH_INACTIVE = 0x80,
};

class Sf3Player {
public:
    Sf3Player(std::unique_ptr<SoundData> _data);

    static std::unique_ptr<Sf3Player> makePlayer(std::unique_ptr<SoundData> _data);
    void SsRequest(int sound);
    void SsRequestPan(int sound, int pan);

    void Step(int steps, s16* out);

private:
    void StepSequencer();
    void StepSynth(s16* out);

    void StepChannel(sndChannel& ch, int idx, bool bgm);

    int playNote(sndChannel& ch, int note, int velocity);
    int readSeqCtrl(sndChannel& ch, int idx, bool bgm);
    void StepSequence(sndChannel& ch, int idx, bool bgm);

    int calcPitch(int pitch);

    u64 sequenceAcc;

    sndChannel bgmChan[16];
    sndChannel sfxChan[16];

    sndPanState chPan[16];
    sndVoice voice[16];

    u32 bgmTempo;
    u32 channelTempo[16];
    u8 seqStatus[16];

    std::unique_ptr<SoundData> data;
};

#endif // PLAYER_H_
