#ifndef PLAYER_H_
#define PLAYER_H_

#include "snd_data.h"

#include <memory>

enum chFlag {
    CH_LFO = 0x1,
    CH_PORTAMENTO = 0x2,
    CH_DELAY = 0x20,
    CH_END = 0x40,
    CH_INACTIVE = 0x80,
};

struct sndChannel {
    u8* seq_ptr = nullptr;
    u8* sequence = nullptr;
    Tone* tone = nullptr;
    Sample* sample = nullptr;

    int pitch = 0;
    int currentPitch = 0;
    double duration = 0; // duration of current note
    double delay = 0; // delay until next sequence event
    int tremoloLevel = 0;

    u8* loopPoint[4] = { nullptr, nullptr, nullptr, nullptr };

    ushort vibrato = 0;
    ushort tremolo = 0;
    ushort lfoRate = 0;
    ushort volAdjust = 0;
    short transpose = 0;
    ushort portamento_unk44 = 0;
    ushort portamento_unk46 = 0;
    ushort attackStep = 0;
    ushort attackTarget = 0;
    ushort decayStep = 0;
    ushort sustainStep = 0;
    ushort sustainTarget = 0;
    ushort releaseStep = 0;

    // changed to int to simplify envelope checking
    int envLevel = 0;

    u8 envState = 0;
    u8 newNote = 0;
    u8 noteActive = 0;
    u8 expression = 0x40;
    u8 unk65 = 0;
    u8 unk66 = 0;
    u8 unk6a = 0;
    s8 loopCount[4] = { 0, 0, 0, 0 };
    u8 note = 0;
    u8 chFlags = CH_INACTIVE | CH_END;
    s8 pitchBend = 0;
    s8 fineTune = 0x40;
    u8 bankId = 0;
    u8 progId = 0;
    s8 volume = 0;
    u8 velocity = 0;
    u8 pan = 0x40;
    u8 seqFlags = 0;
    u8 wasHeld = 0;
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

	s32 s[2];

    void keyOn()
    {
		s[0] = 0;
		s[1] = 0;
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
    u16 val;
    s16 target;
    s16 step;
    s16 mode;
};

class Sf3Player {
public:
    Sf3Player(std::unique_ptr<SoundData> _data);

    static std::unique_ptr<Sf3Player> makePlayer(std::unique_ptr<SoundData> _data);
    void SsBgmOff();
    void SsRequest(int sound);
    void SsQueue(int sound);

    void Step(int steps, s16* out);

private:
    void StepSequencer();
    void StepSynth(s16* out);
    void requestSound(int sound, int pan);

    void StepChannel(sndChannel& ch, int idx, bool bgm);

    int playNote(sndChannel& ch, int note, int velocity);
    int readSeqCtrl(sndChannel& ch, int idx, bool bgm);
    void StepSequence(sndChannel& ch, int idx, bool bgm);

    int calcPitch(int pitch);
    int calcVol(int env, int lfo, s8 unk, sndChannel& ch);

    u64 sequenceAcc = 0;

    sndChannel bgmChan[16];
    sndChannel sfxChan[16];

    sndPanState chPan[16];
    sndVoice voice[16];

    u8 bgmVolume = 0;
    double bgmTempo = 0;
    u32 channelTempo[16];
    u8 seqStatus[16];

    bool bgmOn = 0;
    bool stereo = 1;
    int queue = -1;

    u64 tick = 0;
    std::unique_ptr<SoundData> data;
};

#endif // PLAYER_H_
