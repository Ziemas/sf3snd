#include "cps3snd.h"

#include "loader.h"
#include "player.h"

struct cps3Snd : public cps3Player {
};

struct cps3Snd*
cps3snd_player_create(void* code, size_t code_size, void* data, size_t data_size)
{
    auto bank = loadSoundData({ (u8*)code, code_size }, { (u8*)data, data_size });
    return new cps3Snd(std::move(bank));
}

void cps3snd_player_destroy(struct cps3Snd* snd)
{
    delete snd;
}

void cps3snd_play_sound(struct cps3Snd* snd, int sound)
{
    snd->SsRequest(sound);
}

void cps3snd_queue_bgm(struct cps3Snd* snd, int sound)
{
    snd->SsQueue(sound);
}

void cps3snd_bgm_off(struct cps3Snd* snd)
{
	snd->SsBgmOff();
}

void cps3snd_run(struct cps3Snd* snd, int sample_count, int16_t* samples_out)
{
    snd->Step(sample_count, samples_out);
}
