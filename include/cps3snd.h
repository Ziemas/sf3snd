#ifndef CPS3SND_H_
#define CPS3SND_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cps3Snd;

struct cps3Snd* cps3snd_player_create(void* code, size_t code_size, void* data, size_t data_size);
void cps3snd_player_destroy(struct cps3Snd* snd);
void cps3snd_play_sound(struct cps3Snd* snd, int sound);
void cps3snd_queue_bgm(struct cps3Snd* snd, int sound);
void cps3snd_bgm_off(struct cps3Snd* snd);
void cps3snd_run(struct cps3Snd* snd, int sample_count, int16_t* samples_out);

#ifdef __cplusplus
}
#endif

#endif // CPS3SND_H_
