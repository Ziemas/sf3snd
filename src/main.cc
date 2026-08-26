#include "BinaryReader.h"
#include "common.h"
#include "loader.h"
#include "player.h"
#include "snd_data.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <memory>
#include <optional>
#include <print>
#include <readline/history.h>
#include <readline/readline.h>
#include <unordered_map>
#include <vector>

std::unique_ptr<Sf3Player> player;
static SDL_AudioStream* stream;
SDL_Mutex* soundLock;

std::unordered_map<std::string, int (*)(int, char**)> commands;

static int command_run(char* s)
{
    char** tokens = history_tokenize(s);
    int i;

    for (i = 0; tokens[i]; i++)
        ;

    auto cmd = commands.find(tokens[0]);
    if (cmd != commands.end()) {
        cmd->second(i, tokens);
    }

    for (i = 0; tokens[i]; i++) {
        free(tokens[i]);
    }

    free(tokens);
    return 0;
}

static void command_line(char* line)
{
    char* hist_expand;
    int ret;

    if (!line) {
        exit(0);
        return;
    }

    if (!line[0]) {
        goto exit;
    }

    ret = history_expand(line, &hist_expand);
    if (ret >= 0 && ret != 2) {
        add_history(hist_expand);
        command_run(hist_expand);
    }

    free(hist_expand);

exit:
    free(line);
}

static int command_loop()
{
    std::string prompt = "sf3snd: ";

    while (true) {
        char* line = readline(prompt.c_str());
        command_line(line);
    }
}

static int playSound(int ac, char** tokens)
{
    if (ac < 2) {
        return -1;
    }

    int idx;
    auto res = std::from_chars(tokens[1], tokens[1] + strlen(tokens[1]), idx);
    if (res.ec != std::errc {}) {
        std::println("?");
        return -1;
    }

    SDL_LockMutex(soundLock);
    player->SsRequest(idx);
    SDL_UnlockMutex(soundLock);

    return 0;
}

void SDL_CB(void* user, SDL_AudioStream* stream, int additional_amount, int total_amount)
{
    (void)total_amount;
    (void)user;

    SDL_LockMutex(soundLock);

    u32 samples_per_channel = (additional_amount / sizeof(s16)) >> 1;
    static s16 outbuf[4096] = {};

    while (samples_per_channel) {
        u32 batch_count = std::min<u32>(samples_per_channel, 4096);
        s16* p = outbuf;
        player->Step(batch_count, p);

        SDL_PutAudioStreamData(stream, outbuf, (batch_count * sizeof(s16)) << 1);
        samples_per_channel -= batch_count;
    }

    SDL_UnlockMutex(soundLock);
}

int startAudio()
{
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    SDL_Init(SDL_INIT_AUDIO);

    soundLock = SDL_CreateMutex();

    SDL_AudioSpec spec;

    spec.channels = 2;
    spec.format = SDL_AUDIO_S16;
    spec.freq = 37286;

    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, SDL_CB, NULL);
    if (!stream) {
        SDL_Log("Couldn't create SDL audio stream: %s", SDL_GetError());
        return -1;
    }

    SDL_ResumeAudioStreamDevice(stream);

    return 0;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    std::vector<u8> code = read_binary_file("decrypted.bin");
    std::vector<u8> data = read_binary_file("user5.bin");

    auto soundData = loadSoundData(code, data);
    player = Sf3Player::makePlayer(std::move(soundData));

    if (startAudio() < 0) {
        return -1;
    }

    commands["play"] = playSound;

    SDL_LockMutex(soundLock);
    player->SsRequest(3);
    SDL_UnlockMutex(soundLock);

    command_loop();

    return 0;
}
