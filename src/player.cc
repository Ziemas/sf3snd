#include "player.h"
#include <print>

// Delay is stored in sort of inverse-vlq
static std::pair<size_t, u32> readDelay(u8* value)
{
    size_t len = 0;
    u32 out = 0;

    while ((*value & 0x80) == 0) {
        out = (out << 7) + *value;
        len++;
        value++;
    }

    return { len, out };
}

// normal MIDI style VLQ
static std::pair<size_t, u32> readVLQ(u8* value)
{
    size_t len = 1;
    u32 out = *value & 0x7f;

    while ((*value & 0x80) != 0) {
        len++;
        value++;
        out = (out << 7) + (*value & 0x7f);
    }

    return { len, out };
}

Sf3Player::Sf3Player(std::unique_ptr<SoundData> _data)
    : data(std::move(_data))
{
    for (auto& ch : bgmChan) {
        ch.chFlags = CH_INACTIVE | CH_END;
    }

    for (auto& ch : sfxChan) {
        ch.chFlags = CH_INACTIVE | CH_END;
    }
};

std::unique_ptr<Sf3Player> Sf3Player::makePlayer(std::unique_ptr<SoundData> _data)
{
    return std::make_unique<Sf3Player>(std::move(_data));
}

void Sf3Player::SsBgmOff()
{
    if (bgmOn) {
        for (auto& ch : bgmChan) {
            ch.chFlags = CH_INACTIVE | CH_END;
        }

        bgmOn = 0;
    }
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
		SsBgmOff();

        for (int i = 0; i < 16; i++) {
            auto& c = bgmChan[i];
            auto& t = snd.track_sequence[i];

            if (t.empty()) {
                c.chFlags = CH_INACTIVE | CH_END;
                continue;
            }

            c = {};

            c.seq_ptr = t.data();

            auto [len, delay] = readDelay(c.seq_ptr);
            c.seq_ptr += len;

            c.sequence = t.data();
            c.delay = delay << 8;
            c.chFlags = CH_DELAY;
            auto* prog = &data->instrumentBank[0].program[0].value();
            c.tone = &prog->instrument[0];
            c.sample = &data->sample[prog->instrument[0].sampleIdx];
            c.seqFlags = snd.flags;
            c.pan = 0x40;
            c.fineTune = 0x40;
            c.unk64 = 0x40;
        }

        bgmOn = 1;
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
    if (bgm && (sfxChan[idx].chFlags & CH_INACTIVE) == 0) {
        commitRegs = 0;
    }

    if (ch.chFlags & CH_INACTIVE) {
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
        ch.chFlags &= ~CH_DELAY;
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
        // TODO
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

    if (ch.chFlags & CH_END) {
        if (ch.envLevel == 0) {
            ch.chFlags |= CH_INACTIVE;
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

    if (!commitRegs) {
        return;
    }

    int pan;
    if (bgm || chPan[idx].mode == -1) {
        pan = ch.tone->pan;
        if (pan == 0xff) {
            pan = ch.pan;
        }
    } else {
        pan = chPan[idx].start >> 8;
        ;
    }

    vc.voll = 0x1fff;
    vc.volr = 0x1fff;

    int pitch = ch.currentPitch + (ch.pitchBend * 0xc00 >> 7) + ((ch.fineTune - 0x40) * 0x100 >> 6);

    vc.pitch = calcPitch(pitch);

    if (keyOn) {
        vc.keyOn();
    }
}

int Sf3Player::playNote(sndChannel& ch, int note, int velocity)
{
    ch.note = note;
    ch.velocity = velocity;

    auto& bank = data->instrumentBank[ch.bankId];
    if (!bank.program[ch.progId].has_value()) {
        return -1;
    }

    auto& prog = bank.program[ch.progId].value();
    if (prog.instrument.empty()) {
        return -1;
    }

    Tone* t = nullptr;
    for (auto& i : prog.instrument) {
        if (note <= i.range) {
            t = &i;
            break;
        }
    }

    if (t == nullptr) {
        return -1;
    }

    ch.tone = t;
    ch.sample = &data->sample[t->sampleIdx];
    ch.pitch = ((note - ch.sample->key) + ch.transpose + 7) * 0x100 + 0x80 + ch.tone->fineTune;
    ch.attackTarget = velocity << 8;
    ch.sustainTarget = ((t->sustainLevel + 1) * ch.attackTarget) >> 7;
    ch.attackStep = envRate(ch.attackTarget, t->attackRate, data->ar_table);
    ch.attackStep = ch.attackStep * (ch.velocity + 1) >> 7;
    ch.decayStep = envRate(ch.attackTarget, t->decayRate, data->dr_table);
    ch.sustainStep = envRate(ch.sustainTarget, t->sustainRate, data->dr_table);

    return 0;
}

int Sf3Player::readSeqCtrl(sndChannel& ch, int idx, bool bgm)
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
        // std::println("volume {:x}", ch.volume);
        ch.seq_ptr += 2;
        break;
    case 0xc7:
        ch.pan = ch.seq_ptr[1];
        // std::println("pan {:x}", ch.pan);
        ch.seq_ptr += 2;
        break;
    case 0xc8:
        ch.expression = ch.seq_ptr[1];
        // std::println("expression {:x}", ch.expression);
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
            ch.chFlags &= ~CH_PORTAMENTO;
            ch.portamento_unk46 = 0;
        }
    } break;
    case 0xca:
        if (ch.unk65 == 0) {
            ch.seq_ptr = ch.sequence;
            ch.unk65 = 1;
        }
        ch.seq_ptr += 1;
        break;
    case 0xcb:
        if (ch.unk65) {
            ch.chFlags |= CH_END;
            return -1;
        }
        ch.seq_ptr += 1;
        break;
    case 0xcc:
        if (ch.unk65 == 0) {
            ch.seq_ptr += (ch.seq_ptr[1] << 8) + ch.seq_ptr[2];
            ch.unk65 = 1;
        }
        ch.seq_ptr += 3;
        break;
    case 0xcd:
        if (ch.unk65) {
            ch.seq_ptr += (ch.seq_ptr[1] << 8) + ch.seq_ptr[2];
        }
        ch.seq_ptr += 3;
        break;
    case 0xce: {
        short offset = (ch.seq_ptr[1] << 8) + ch.seq_ptr[2];
        ch.seq_ptr += 3;
        ch.seq_ptr += offset;
    } break;
    case 0xcf:
        ch.seq_ptr = bgmChan[ch.seq_ptr[1]].sequence;
        break;
    case 0xd0:
    case 0xd1:
    case 0xd2:
    case 0xd3: {
        // loop start
        int idx = status - 0xd0;
        ch.loopPoint[idx] = ch.seq_ptr + 1;
        ch.seq_ptr += 1;
    } break;
    case 0xd4:
    case 0xd5:
    case 0xd6:
    case 0xd7: {
        // loop end
        int idx = status - 0xd4;

        if (ch.loopCount[idx] == 0) {
            ch.loopCount[idx] = ch.seq_ptr[1];
        } else {
            ch.loopCount[idx]--;
            if (ch.loopCount[idx] == 0) {
                ch.seq_ptr += 2;
                break;
            }
        }
        ch.seq_ptr = ch.loopPoint[idx];
    } break;
    case 0xd8:
    case 0xd9:
    case 0xda:
    case 0xdb: {
        int idx = status - 0xd8;
        if (ch.loopCount[idx] == 1) {
            ch.loopCount[idx] = 0;
            ch.seq_ptr += (ch.seq_ptr[1] << 8) + ch.seq_ptr[2];
        }

        ch.seq_ptr += 3;
    } break;
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
            ch.chFlags |= CH_LFO;
        } else {
            ch.chFlags &= ~CH_LFO;
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
        ch.chFlags |= CH_END;
        return -1;
        break;
    default:
        std::println("[ch{}] Unrecognized status {:x} {:x}", idx, status, ch.chFlags);
        ch.chFlags |= (CH_END | CH_INACTIVE);
        return -1;
    }

    return 0;
}

void Sf3Player::StepSequence(sndChannel& ch, int idx, bool bgm)
{
    if (ch.chFlags & (CH_END | CH_DELAY)) {
        return;
    }

    if (idx == 8 && ch.chFlags & CH_END) {
        std::println("???");
    }

    while (1) {
        u8 status = *ch.seq_ptr;

        if (status < 0xc0) {
            int velocity = (ch.seq_ptr[0] & 0x3f) << 1;
            int note = ch.seq_ptr[1] & 0x7f;
            int unkMsb = ch.seq_ptr[1] & 0x80;

            std::println("[ch{}] note: {:x}, vel: {:x}", idx, note, velocity);

            int res = playNote(ch, note, velocity);
            if (!res) {
                ch.newNote = 1;

                if (ch.unk6a == 0) {
                    ch.unk66 = 1;
                } else {
                    ch.unk66 = 0;
                }

                if (unkMsb == 0) {
                    ch.noteActive = 1;
                    ch.unk6a = 0;
                } else {
                    ch.noteActive = 0;
                    ch.unk6a = 1;
                }
            } else {
                ch.newNote = 0;
                ch.noteActive = 0;
                ch.unk66 = 0;
                ch.unk6a = 0;
            }

            ch.seq_ptr += 2;
            auto [len, duration] = readVLQ(ch.seq_ptr);
            ch.seq_ptr += len;
            ch.duration = duration << 8;
        } else {
            int ret = readSeqCtrl(ch, idx, bgm);
            if (ret < 0) {
                return;
            }
        }

        auto [len, delay] = readDelay(ch.seq_ptr);
        ch.delay += delay << 8;
        ch.seq_ptr += len;

        if (delay > 0) {
            break;
        }
    }

    ch.chFlags |= CH_DELAY;
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
    int accl = 0, accr = 0;

    for (auto& v : voice) {
        if (!v.key) {
            continue;
        }

        int size = v.sample->size();

        int s1 = (*v.sample)[v.pos] << 8;
        int s2 = (*v.sample)[(v.pos + 1) % size] << 8;

        // linear interpolation, figure out if people like it i guess
        int sample = (s1 * (0xfff - v.counter) + s2 * v.counter) >> 12;

        accl += (sample * v.voll) >> 15;
        accr += (sample * v.volr) >> 15;

        v.counter += v.pitch;
        v.pos += v.counter >> 12;
        v.counter &= 0xfff;

        if (v.pos >= v.sample->size()) {
            if (v.loop) {
                v.pos = 0;
            } else {
                v.key = 0;
            }
        }
    }

    out[0] = std::clamp(accl, -0x8000, 0x7fff);
    out[1] = std::clamp(accr, -0x8000, 0x7fff);
}

void Sf3Player::Step(int steps, s16* out)
{
    while (steps) {
        sequenceAcc += 59599491;

        if (sequenceAcc >= 37286000000) {
            sequenceAcc -= 37286000000;
            StepSequencer();
        }

        StepSynth(out);

        out += 2;
        steps--;
    }
}
