#include "player.h"
#include <print>

// Delay is stored in sort of inverse-vlq
static std::pair<size_t, u32> readDelay(u8* value)
{
    size_t len = 0;
    u32 out = 0;

    if ((*value & 0x80) == 0) {
        while ((*value & 0x80) == 0) {
            len++;
            value++;
            out = (out << 7) + (*value & 0x7f);
        }
    }

    return { len, out };
}

// normal MIDI style VLQ
static std::pair<size_t, u32> ReadVLQ(u8* value)
{
    size_t len = 1;
    u32 out = *value & 0x7f;

    if ((*value & 0x80) != 0) {
        while ((*value & 0x80) != 0) {
            len++;
            value++;
            out = (out << 7) + (*value & 0x7f);
        }
    }

    return { len, out };
}

std::unique_ptr<Sf3Player> Sf3Player::makePlayer(std::unique_ptr<SoundData> _data)
{
    return std::make_unique<Sf3Player>(std::move(_data));
}

void Sf3Player::SsRequest(int sound)
{
    SsRequestPan(sound, -1);
}

void Sf3Player::SsRequestPan(int sound, int pan)
{
    auto it = data->sound.find(sound);
    if (it == data->sound.end()) {
        std::println("non-existant sound");
        return;
    }

    Sound& snd = it->second;
    if (snd.flags == 0) {
        // does this mean BGM?
        for (int i = 0; i < 16; i++) {
            auto& c = bgm_chan[i];
            auto& t = snd.track_sequence[i];
            if (t.empty()) {
                c.flags = 0xc0;
                continue;
            }

            c.seq_ptr = t.data();

            auto [len, delay] = readDelay(c.seq_ptr);
            c.seq_ptr += len;

            c.sequence = t.data();
            c.delay = delay << 8;
            c.flags = SEQ_DELAY;
            c.prog = &data->instrumentBank[0].program[0].value();
            c.sample = &data->sample[c.prog->instrument[0].sampleIdx];
        }
    }
}

void Sf3Player::StepChannel(sndChannel& ch, int voice)
{
    if (ch.flags & (SEQ_END | SEQ_DELAY)) {
        return;
    }

    ch.flags |= SEQ_DELAY;
}

void Sf3Player::StepSequencer()
{
    for (int i = 0; i < 16; i++) {
        StepChannel(bgm_chan[i], i);
    }
}

void Sf3Player::Step(int steps, s16* out)
{
    // 37286 / 250 = 149
    static int cb_timer = 149;

    while (steps) {
        cb_timer--;
        if (!cb_timer) {
            StepSequencer();
            cb_timer = 149;
        }

        steps--;
    }
}
