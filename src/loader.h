#ifndef LOADER_H_
#define LOADER_H_

#include "BinaryReader.h"
#include "common.h"
#include "snd_data.h"

#include <memory>
#include <span>
#include <vector>

std::vector<u8> read_binary_file(const char* path);
std::unique_ptr<SoundData> loadSoundData(std::span<u8> code, std::span<u8> data);

#endif // LOADER_H_
