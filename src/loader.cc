#include "loader.h"
#include "BinaryReader.h"
#include "snd.h"

#include <cstdio>
#include <memory>

static void readSamples(Sample& s, BinaryReader& r, std::span<u8> data)
{
    u32 start_addr = std::byteswap(r.read<u32>());
    u32 loop_addr = std::byteswap(r.read<u32>());
    u32 end_addr = std::byteswap(r.read<u32>());
    u32 key = std::byteswap(r.read<u32>());

    u32 size = end_addr - start_addr;

    s.pcm.resize(size);
    std::memcpy(s.pcm.data(), &data[start_addr], size);

    s.loopAddr = loop_addr - start_addr;
    s.key = key;
}

static Sound readSequence(BinaryReader& r)
{
    Sound s {};
    u32 pos = r.get_seek();

    s.flags = r.read<u8>();

    u16 offsets[16];
    for (int i = 0; i < 16; i++) {
        offsets[i] = std::byteswap(r.read<u16>());
    }

    for (int i = 0; i < 16; i++) {
        if (offsets[i]) {
            r.set_seek(pos + offsets[i]);

            u8 sq;
            while ((sq = r.read<u8>()) != 0xff) {
                s.track_sequence[i].push_back(sq);
            }
        }
    }

    return s;
}

static void readSounds(SoundData& sd, BinaryReader& r)
{
    u32 pos = r.get_seek();

    u16 seq_count = std::byteswap(r.read<u16>());
    sd.unk2 = r.read<u8>();
    sd.unk3 = r.read<u8>();

    for (int i = 0; i < seq_count; i++) {
        u32 offset = std::byteswap(r.read<u32>());

        if (offset) {
            auto t = r.at(pos + offset);
            sd.sound[i] = readSequence(t);
        }
    }
}

static Program readProgram(BinaryReader& r)
{
    Program prog;

    while (r.peek<u16>() != 0xffff) {
        Tone t {};

        t.range = r.read<u8>();
        t.pan = r.read<u8>();
        t.volume = r.read<u8>();
        t.unk = r.read<u8>();
        t.sampleIdx = std::byteswap(r.read<u16>());
        t.fineTune = r.read<u8>();
        t.attackRate = r.read<u8>();
        t.decayRate = r.read<u8>();
        t.sustainLevel = r.read<u8>();
        t.sustainRate = r.read<u8>();
        t.releaseRate = r.read<u8>();

        prog.instrument.push_back(t);
    }

    return prog;
}

static void readBank(Bank& b, BinaryReader& r)
{
    u16 prog_offset[128];
    u32 b_offset = r.get_seek();

    for (auto& p : prog_offset) {
        p = std::byteswap(r.read<u16>());
    }

    for (int i = 0; i < 128; i++) {
        auto pr = r.at(b_offset + prog_offset[i]);
        if (prog_offset[i]) {
            b.program[i] = readProgram(pr);
        } else {
            b.program[i] = std::nullopt;
        }
    }
}

static void readBanks(SoundData& sd, BinaryReader& r)
{
    u32 offsets[16];

    for (auto& off : offsets) {
        off = std::byteswap(r.read<u32>()) & 0xff'ffff;
    }

    for (int i = 0; i < 16; i++) {
        r.set_seek(offsets[i]);
        readBank(sd.instrumentBank[i], r);
    }
}

std::unique_ptr<SoundData> loadSoundData(std::span<u8> code, std::span<u8> data)
{
    auto sd = std::make_unique<SoundData>();

    BinaryReader cr(code);

    cr.set_seek(0x78c000);
    for (auto& s : sd->sample) {
        readSamples(s, cr, data);
    }

    cr.set_seek(0x78f000);
    readSounds(*sd, cr);

    cr.set_seek(0x788000);
    readBanks(*sd, cr);

    return sd;
}

std::vector<uint8_t> read_binary_file(const char* path)
{
    FILE* fp = fopen(path, "r");

    fseek(fp, 0, SEEK_END);
    auto len = ftell(fp);
    if (len == 0) {
        fclose(fp);
        return {};
    }
    rewind(fp);

    std::vector<uint8_t> data;
    data.resize(len);

    fread(data.data(), len, 1, fp);
    fclose(fp);

    return data;
}
