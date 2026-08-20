#include "BinaryReader.h"
#include "common.h"
#include "loader.h"
#include "snd.h"

#include <cstdio>
#include <memory>
#include <optional>
#include <print>
#include <readline/history.h>
#include <readline/readline.h>
#include <unordered_map>
#include <vector>

std::unique_ptr<SoundData> soundData;

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

int listSample(int ac, char** tokens)
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

    auto& s = soundData->sample[idx];

	std::println("{:x}", s.loopAddr);
	std::println("{:x}", s.key);
	std::println("{:x}", s.pcm.size());
	std::println("{:x}", s.pcm[0]);

    return 0;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    std::vector<u8> code = read_binary_file("decrypted.bin");
    std::vector<u8> data = read_binary_file("user5.bin");

    soundData = loadSoundData(code, data);

    commands["sample"] = listSample;

    command_loop();

    return 0;
}
