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
            auto& c = bgmChan[i];
            auto& t = snd.track_sequence[i];
            if (t.empty()) {
                c.flags = CH_INACTIVE | CH_END;
                continue;
            }

            c.seq_ptr = t.data();

            auto [len, delay] = readDelay(c.seq_ptr);
            c.seq_ptr += len;

            c.sequence = t.data();
            c.delay = delay << 8;
            c.flags = CH_DELAY;
            auto* prog = &data->instrumentBank[0].program[0].value();
            c.tone = &prog->instrument[0];
            c.sample = &data->sample[prog->instrument[0].sampleIdx];
            c.seqFlags = snd.flags;
        }
    }
}

int Sf3Player::calcPitch(int pitch)
{
    int ret = 0;
    int unk = 0;

    for (; 0xc00 <= pitch; pitch -= 0xc00) {
        unk += 1;
    }

    for (; pitch < 0; pitch += 0xc00) {
        unk -= 1;
    }

    ret = data->freq_table[pitch];

    for (; 0 < unk; unk -= 1) {
        ret <<= 1;
    }

    for (; unk < 0; unk += 1) {
        ret = (ret & 0xffff) >> 1;
    }

    return ret - 1;
}

static int envRate(int value, int rate, ushort* table)
{
    int ret = (table[rate & 0xff] + 1) * (value & 0xffff) >> 0x10;

    if (!ret) {
        return 1;
    }

    return ret;
}

void Sf3Player::StepChannel(sndChannel& ch, int idx, bool bgm)
{
    auto& vc = voice[idx];

    bool keyOn = 0;
    bool commitRegs = 1;

    // If theres a sound effect using the channel, it should have priority
    if (bgm && (sfxChan[idx].flags & CH_INACTIVE) == 0) {
        commitRegs = 0;
    }

    if (ch.flags & CH_INACTIVE) {
        if (!bgm || !commitRegs) {
            return;
        }

        vc.keyOff();

        return;
    }

    if (bgm) {
        ch.delay -= bgmTempo;
    } else {
        ch.delay -= channelTempo[idx];
    }

    if (ch.noteActive) {
        if (bgm) {
            ch.duration -= bgmTempo;
        } else {
            ch.duration -= channelTempo[idx];
        }
    }

    if (ch.delay < 1) {
        ch.flags &= ~CH_DELAY;
    }

    if (ch.duration < 1 && ch.noteActive) {
        ch.noteActive = 0;
        ch.unk68 = 0;

        if (commitRegs) {
            vc.loop = 0;
        }

        // Go into release
        if (ch.envState != 4) {
            ch.envState = 4;
            ch.releaseStep = envRate(ch.envLevel, ch.tone->releaseRate, data->dr_table);
        }
    }

    if (ch.newNote) {
        ch.newNote = 0;
        ch.currentPitch = ch.pitch;
        if (ch.unk66 == 0) {
            // TODO
            ch.envState = 3;
        } else {
            ch.unk66 = 0;
            ch.unk68 = 1;
            ch.envLevel = 0;
            if (commitRegs) {
                vc.keyOff();
                vc.sample = &ch.sample->pcm;
                vc.loopAddr = ch.sample->loopAddr;
                vc.loop = ch.sample->loopAddr != ch.sample->pcm.size();
				keyOn = 1;
            }

            // TODO

            ch.envState = 1;

            // TODO
        }
    }

    // TODO bgm thing here

    if (ch.flags & CH_END) {
        if (ch.envLevel == 0) {
            ch.flags |= CH_INACTIVE;
            ch.seqFlags = 0;
            return;
        }

        ch.envState = 4;
        if (commitRegs) {
            vc.loop = 0;
        }
    }

    // TODO portamento

    switch (ch.envState) {
    case 1:
        ch.envLevel += ch.attackStep;
        if (ch.envLevel >= ch.attackTarget) {
            ch.envLevel = ch.attackTarget;
            ch.envState = 2;
        }
        break;
    case 2:
        ch.envLevel -= ch.decayStep;
        if (ch.envLevel < ch.sustainTarget) {
            ch.envLevel = ch.sustainTarget;
            ch.envState = 3;
        }
        break;
    case 3:
        ch.envLevel -= ch.sustainStep;
        if (ch.envLevel <= 0) {
            ch.envLevel = 0;
            ch.envState = 0;
        }
        break;
    case 4:
        ch.envLevel -= ch.releaseStep;
        if (ch.envLevel <= 0) {
            ch.envLevel = 0;
            ch.envState = 0;
            if (bgm && commitRegs) {
                vc.keyOff();
            }
        }
        break;
    }

    // TODO LFO

    // TODO pan

	vc.voll = 0x7fff;
	vc.volr = 0x7fff;

    int pitch = 0;
    if (bgm) {
        pitch = ch.currentPitch;
    } else {
        pitch = ch.currentPitch;
    }

    vc.pitch = calcPitch(pitch);

    if (keyOn) {
		std::println("key on");
        vc.keyOn();
    }
}

void Sf3Player::playNote(sndChannel& ch, int note, int velocity)
{
    ch.note = note;
    ch.velocity = velocity;

    auto prog = data->instrumentBank[ch.bankId].program[ch.progId];
    if (prog.has_value() && !prog.value().instrument.empty()) {
    }
}

void Sf3Player::readSeqCtrl(sndChannel& ch, int idx, bool bgm)
{
    u8 status = *ch.seq_ptr;

    std::println("[ch{}] status: {:x}", idx, status);

    switch (status) {
    case 0xc0:
        break;
    case 0xc1:
        if (bgm) {
            bgmTempo = (ch.seq_ptr[1] << 8) + ch.seq_ptr[2];
        } else {
            channelTempo[idx] = (ch.seq_ptr[1] << 8) + ch.seq_ptr[2];
        }
        ch.seq_ptr += 3;
        break;
    case 0xc2:
        ch.bankId = ch.seq_ptr[1] & 0xf;
        ch.seq_ptr += 2;
        break;
    case 0xc3:
        ch.pitchBend = ch.seq_ptr[1];
        ch.seq_ptr += 2;
        break;
    case 0xc4:
        ch.progId = ch.seq_ptr[1] & 0x7f;
        ch.seq_ptr += 2;
        break;
    case 0xc5:
        ch.vibrato = data->vibrato_table[ch.seq_ptr[1]];
        ch.seq_ptr += 2;
        break;
    case 0xc6:
        ch.volume = ch.seq_ptr[1];
        ch.seq_ptr += 2;
        break;
    case 0xc7:
        ch.pan = ch.seq_ptr[1];
        ch.seq_ptr += 2;
        break;
    case 0xc8:
        ch.expression = ch.seq_ptr[1];
        ch.seq_ptr += 2;
        break;
    case 0xc9: {
        ch.portamento_unk44 = ch.portamento_unk46;
        u8 value = ch.seq_ptr[1];
        ch.seq_ptr += 2;

        if (value) {
            ch.portamento_unk46 = (value + 1) * 2;
            ch.portamento_unk44 = ch.portamento_unk46;
        } else {
            ch.flags &= ~CH_PORTAMENTO;
            ch.portamento_unk46 = 0;
        }
    } break;
    case 0xca:
        // TODO
        // ch.seq_ptr = ch.sequence;
        ch.seq_ptr += 1;
        break;
    case 0xcb:
        // TODO
        // ch.flags |= SEQ_END;
        ch.seq_ptr += 1;
        break;
    case 0xcc:
        // TODO
        ch.seq_ptr += 3;
        break;
    case 0xcd:
        // TODO
        ch.seq_ptr += 3;
        break;
    case 0xce: {
        short offset = (ch.seq_ptr[1] << 8) + ch.seq_ptr[2];
        ch.seq_ptr += 3;
        ch.seq_ptr += offset;
    } break;
    case 0xcf:
        // goto other channel seq ptr?
        // ch.seq_ptr = ch.sequence;
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
    case 0xdc:
        ch.transpose = (s8)ch.seq_ptr[1];
        ch.seq_ptr += 2;
        break;
    case 0xdd:
        ch.transpose += (s8)ch.seq_ptr[1];
        ch.seq_ptr += 2;
        break;
    case 0xde:
        ch.unk58 = ch.seq_ptr[1];
        ch.seq_ptr += 2;
        break;
    case 0xdf:
        ch.unk58 += (s8)ch.seq_ptr[1];
        ch.seq_ptr += 2;
        break;
    case 0xe0:
        if (ch.seq_ptr[1]) {
            ch.flags |= CH_UNK1;
        } else {
            ch.flags &= ~CH_UNK1;
        }

        ch.seq_ptr += 2;
        break;
    case 0xe1:
        ch.lfoRate = data->tremolo_table[ch.seq_ptr[1]];
        ch.seq_ptr += 2;
        break;
    case 0xe2:
        ch.tremolo = data->tremolo_table[ch.seq_ptr[1]];
        ch.seq_ptr += 2;
        break;
    case 0xe3:
        ch.seqFlags = ch.seq_ptr[1];
        ch.seq_ptr += 2;
        break;
    case 0xe4:
    case 0xe5:
        // nop
        ch.seq_ptr += 3;
        break;
    case 0xe6:
        // nop
        ch.seq_ptr += 2;
        break;
    case 0xe7:
        ch.fineTune = ch.seq_ptr[1];
        ch.seq_ptr += 2;
        break;
    case 0xe8:
        seqStatus[ch.seq_ptr[1]] = ch.seq_ptr[2];
        ch.seq_ptr += 3;
        break;
    case 0xff:
        ch.flags |= CH_END;
        break;
    default:
        std::println("Unrecognized status {:x}", status);
        break;
    }
}

void Sf3Player::StepSequence(sndChannel& ch, int idx, bool bgm)
{
    if (ch.flags & (CH_END | CH_DELAY)) {
        return;
    }

    while (1) {
        u8 status = *ch.seq_ptr;

        if (status < 0xc0) {
            int velocity = (ch.seq_ptr[0] & 0x3f) << 1;
            int note = ch.seq_ptr[1] & 0x7f;

            std::println("[ch{}] note: {:x}, vel: {:x}", idx, ch.note, ch.velocity);

            playNote(ch, note, velocity);

            ch.seq_ptr += 2;
            auto [len, duration] = readVLQ(ch.seq_ptr);
            ch.seq_ptr += len;
            ch.duration = duration << 8;
        } else {
            readSeqCtrl(ch, idx, bgm);
        }

        auto [len, delay] = readDelay(ch.seq_ptr);
        ch.delay += delay << 8;
        ch.seq_ptr += len;

        if (delay > 0) {
            break;
        }
    }

    ch.flags |= CH_DELAY;
}

void Sf3Player::StepSequencer()
{
    for (int i = 0; i < 16; i++) {
        StepSequence(bgmChan[i], i, true);
    }

    for (int i = 0; i < 16; i++) {
        if (!(sfxChan[i].seqFlags & 0x80)) {
            // StepSequence(sfx_chan[i], i, true);
        }
    }

    for (int i = 0; i < 16; i++) {
        StepChannel(bgmChan[i], i, true);
    }

    for (int i = 0; i < 16; i++) {
        if (!(sfxChan[i].seqFlags & 0x80)) {
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
        sequenceAcc += 59599491;

        if (sequenceAcc >= 37286000000) {
            StepSequencer();
            sequenceAcc = 0;
        }

        StepSynth(out);

        out += 2;
        steps--;
    }
}
