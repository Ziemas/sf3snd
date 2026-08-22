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
static std::pair<size_t, u32> readVLQ(u8* value)
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
                c.flags = SEQ_INACTIVE | SEQ_END;
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

void Sf3Player::StepChannel(sndChannel& ch, int idx, bool bgm)
{
    auto& vc = voice[idx];

    if (ch.flags & SEQ_INACTIVE) {
        vc.key = 0;
        return;
    }

    if (bgm) {
        ch.delay -= bgm_tempo;
    } else {
        ch.delay -= channel_tempo[idx];
    }

    if (1 /*unk 0x67*/) {
        if (bgm) {
            ch.duration -= bgm_tempo;
        } else {
            ch.duration -= channel_tempo[idx];
        }
    }

    if (ch.delay < 1) {
        ch.flags &= ~SEQ_DELAY;
    }
}

void Sf3Player::StepSequence(sndChannel& ch, int idx, bool bgm)
{
    if (ch.flags & (SEQ_END | SEQ_DELAY)) {
        return;
    }

    while (1) {
        u8 status = *ch.seq_ptr;

        if (status < 0xc0) {
            std::println("[ch{}] note: {:x}", idx, status);

			ch.velocity = (ch.seq_ptr[0] & 0x3f) << 1;
			ch.note = ch.seq_ptr[1] & 0x7f;

            ch.seq_ptr += 2;
            auto [len, duration] = readVLQ(ch.seq_ptr);
            ch.seq_ptr += len;
            ch.duration = duration << 8;
        } else {
            std::println("[ch{}] status: {:x}", idx, status);

            switch (status) {
            case 0xc0:
                break;
            case 0xc1:
                if (bgm) {
                    bgm_tempo = (ch.seq_ptr[1] << 8) + ch.seq_ptr[2];
                } else {
                    channel_tempo[idx] = (ch.seq_ptr[1] << 8) + ch.seq_ptr[2];
                }
                ch.seq_ptr += 3;
                break;
            case 0xc2:
                ch.bankId = ch.seq_ptr[1] & 0xf;
                ch.seq_ptr += 2;
                break;
            case 0xc3:
                ch.seq_ptr += 2;
                break;
            case 0xc4:
				ch.progId = ch.seq_ptr[1] & 0x7f;
                ch.seq_ptr += 2;
                break;
            case 0xc5:
                ch.seq_ptr += 2;
                break;
            case 0xc6:
                ch.seq_ptr += 2;
                break;
            case 0xc7:
                ch.seq_ptr += 2;
                break;
            case 0xc8:
                ch.seq_ptr += 2;
                break;
            case 0xc9:
                ch.seq_ptr += 2;
                break;
            case 0xca:
                ch.seq_ptr = ch.sequence;
                break;
            case 0xcb:
                ch.flags |= SEQ_END;
                break;
            case 0xcc:
                ch.seq_ptr += 3;
                break;
            case 0xcd:
                ch.seq_ptr += 3;
                break;
            case 0xce:
                ch.seq_ptr += 3;
                break;
            case 0xcf:

                break;
            case 0xd0:
            case 0xd1:
            case 0xd2:
            case 0xd3:
                // loop start
                ch.loopPoint[status - 0xd0] = ch.seq_ptr;
				ch.seq_ptr += 1;
                break;
            case 0xd4:
            case 0xd5:
            case 0xd6:
            case 0xd7:
                // loop
                // int loop = status - 0xd4;
                // if (ch.loopFlags[loop] == 0) {
                //	ch.loopFlags[loop] = ch.seq_ptr[1];
                //}
                // ch.seq_ptr = ch.loopPoint[loop];
                ch.seq_ptr += 2;
                break;
            case 0xd8:
            case 0xd9:
            case 0xda:
            case 0xdb:
                ch.seq_ptr += 3;
                break;
            case 0xdd:
                ch.seq_ptr += 2;
                break;
            case 0xde:
                ch.seq_ptr += 2;
                break;
            case 0xdf:
                ch.seq_ptr += 2;
                break;
            case 0xe0:
                ch.seq_ptr += 2;
                break;
            case 0xe1:
                ch.seq_ptr += 2;
                break;
            case 0xe2:
                ch.seq_ptr += 2;
                break;
            case 0xe3:
                ch.seq_ptr += 2;
                break;
            case 0xe4:
                ch.seq_ptr += 3;
                break;
            case 0xe5:
                ch.seq_ptr += 3;
                break;
            case 0xe6:
                ch.seq_ptr += 2;
                break;
            case 0xe7:
                ch.seq_ptr += 2;
                break;
            case 0xe8:
                ch.seq_ptr += 3;
                break;
            case 0xff:
                ch.flags |= SEQ_END;
                break;
            default:
                break;
            }
        }

        auto [len, delay] = readDelay(ch.seq_ptr);
        ch.delay += delay << 8;
        ch.seq_ptr += len;

        if (delay > 0) {
            break;
        }
    }

    ch.flags |= SEQ_DELAY;
}

void Sf3Player::StepSequencer()
{
    for (int i = 0; i < 16; i++) {
        StepSequence(bgm_chan[i], i, true);
    }

    for (int i = 0; i < 16; i++) {
        if (!(sfx_chan[i].flags & SEQ_INACTIVE)) {
            // StepSequence(sfx_chan[i], i, true);
        }
    }

    for (int i = 0; i < 16; i++) {
        StepChannel(bgm_chan[i], i, true);
    }

    for (int i = 0; i < 16; i++) {
        if (!(sfx_chan[i].flags & SEQ_INACTIVE)) {
            // StepChannel(sfx_chan[i], i, true);
        }
    }
}

void Sf3Player::StepSynth(s16* out)
{
    for (auto& v : voice) {
    }
}

void Sf3Player::Step(int steps, s16* out)
{
    while (steps) {
        sequence_acc += 59599491;

        if (sequence_acc >= 37286000000) {
            StepSequencer();
            sequence_acc = 0;
        }

        StepSynth(out);

        out += 2;
        steps--;
    }
}
