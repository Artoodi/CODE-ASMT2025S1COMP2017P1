#include "wav_utils.h"
#include <stdio.h>

// Reads PCM data from a WAV file and stores in dest
void wav_load(const char* fname, int16_t* dest) {
    FILE* file = fopen(fname, "rb");
    if (!file) return;

    char header[4];
    fread(header, 1, 4, file); // "RIFF"
    uint32_t fileSize;
    fread(&fileSize, 4, 1, file); // overall size
    fread(header, 1, 4, file); // "WAVE"

    while (1) {
        fread(header, 1, 4, file);
        uint32_t chunkSize;
        fread(&chunkSize, 4, 1, file);

        if (header[0] == 'd' && header[1] == 'a' && header[2] == 't' && header[3] == 'a') {
            fread(dest, sizeof(int16_t), chunkSize / sizeof(int16_t), file);
            break;
        }
        fseek(file, chunkSize, SEEK_CUR);
        if (feof(file)) break;
    }

    fclose(file);
}

// Writes PCM data to a WAV file
void wav_save(const char* fname, const int16_t* src, size_t len) {
    FILE* file = fopen(fname, "wb");
    if (!file) return;

    const char RIFF[] = "RIFF";
    fwrite(RIFF, 1, 4, file);

    uint32_t fileSize = len * sizeof(int16_t) + 36;
    fwrite(&fileSize, 1, 4, file);

    const char WAVE[] = "WAVE";
    fwrite(WAVE, 1, 4, file);

    const char fmt[] = "fmt ";
    fwrite(fmt, 1, 4, file);

    uint32_t fmtSize = 16;
    fwrite(&fmtSize, 1, 4, file);

    uint16_t audioFormat = 1;
    uint16_t numChannels = 1;
    uint32_t sampleRate = 8000;
    uint32_t byteRate = sampleRate * numChannels * sizeof(int16_t);
    uint16_t blockAlign = numChannels * sizeof(int16_t);
    uint16_t bitsPerSample = 16;

    fwrite(&audioFormat, 1, 2, file);
    fwrite(&numChannels, 1, 2, file);
    fwrite(&sampleRate, 1, 4, file);
    fwrite(&byteRate, 1, 4, file);
    fwrite(&blockAlign, 1, 2, file);
    fwrite(&bitsPerSample, 1, 2, file);

    const char data[] = "data";
    fwrite(data, 1, 4, file);

    uint32_t dataSize = len * sizeof(int16_t);
    fwrite(&dataSize, 1, 4, file);
    fwrite(src, sizeof(int16_t), len, file);

    fclose(file);
}
