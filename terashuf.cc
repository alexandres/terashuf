// Copyright (c) 2017 Salle, Alexandre <atsalle@inf.ufrgs.br>
// Author: Salle, Alexandre <atsalle@inf.ufrgs.br>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include "string.h"
#include "unistd.h"

typedef std::size_t ll;

const int IO_CHUNK = 64 * 1024;
const int LINES_BEFORE_ESTIMATING_MEMORY_OVERHEAD = 1e6;
const char TMP_NAME_TEMPLATE[] = "/terashuftmpXXXXXX";

struct TmpFile
{
    FILE *f;
    char *path;
    ll lines;
    // fields below for bufferedFgetc
    char *buf;
    ll bufPos;
    ll bufN;
    bool eof;
};

ll bufBytes, bufPos;
std::vector<ll> shufIndexes;
char *buf;
float memory = 4.;
char sep = '\n';
int seed = time(NULL);
bool memoryOverheadDisplayed;

ll shufFlushBuf(FILE *f)
{
    std::random_shuffle(shufIndexes.begin(), shufIndexes.end());
    ll bytesWritten = 0;
    for (std::vector<ll>::const_iterator it = shufIndexes.begin(); it != shufIndexes.end(); ++it)
    {
        ll line = *it;
        ll j = 0;
        while (*(buf + line + j) != sep)
            j++;
        ll bytesToWrite = j + 1;
        bytesWritten += bytesToWrite;
        if (fwrite(buf + line, sizeof(char), bytesToWrite, f) !=
            size_t(bytesToWrite))
        {
            fprintf(stderr, "\nFATAL ERROR: failed to write line to disk. is there space left in $TMPDIR?\n");
            exit(1);
        }
    }
    return bytesWritten;
}

char bufferedFgetc(TmpFile *f)
{
    if (!f->eof && f->bufPos == f->bufN)
    {
        f->bufN = fread(f->buf, sizeof(char), IO_CHUNK, f->f);
        f->bufPos = 0;
        if (!f->bufN)
            f->eof = true;
    }
    if (f->eof)
        return EOF;
    return f->buf[f->bufPos++];
}

ll readLine(TmpFile *f)
{
    ll bufPos = 0;
    char c = 0;
    while ((c = bufferedFgetc(f)) != EOF)
    {
        buf[bufPos++] = c;
        if (c == sep)
        {
            return bufPos;
        }
    }
    return 0;
}

void copyLeftOversOrResetBuffer()
{
    if (bufPos < bufBytes)
    {
        // no leftovers, last fillBuf didn't fill buffer
        bufPos = 0;
        return;
    }
    // check if buf ends in newline, otherwise keep track of where
    // incomplete line started so it can be copied to beginning of buffer
    // after flush
    while (bufPos > 0 && buf[bufPos - 1] != sep)
        bufPos--;
    if (bufPos == 0)
    {
        fprintf(
            stderr,
            "FATAL ERROR: line too long to fit in buffer (> %zu bytes):\n",
            bufBytes);
        fwrite(buf, sizeof(char), std::min(bufBytes, (ll)50), stderr);
        fprintf(stderr, "...\n");
        exit(1); // line was too long to fit in buf, can't be shuffled
    }
    ll bytesToCpy = bufBytes - bufPos;
    memcpy(buf, buf + bufPos, bytesToCpy);
    bufPos = bytesToCpy;
}

ll fillBufAndMarkLines(FILE *f)
{
    copyLeftOversOrResetBuffer();

    bufPos += fread(buf + bufPos, sizeof(char), bufBytes - bufPos, f);

    // handle missing sep on last line
    // buffer not empty and not full and previous position is not newline
    if (bufPos > 0 && bufPos < bufBytes && buf[bufPos - 1] != sep)
        buf[bufPos++] = sep;

    // mark lines and store pos in shufIndexes
    ll lineStart = 0;
    shufIndexes.clear();
    for (ll i = 0; i < bufPos; i++)
    {
        if (buf[i] != sep)
            continue;
        shufIndexes.push_back(lineStart);
        lineStart = i + 1;
        if (!memoryOverheadDisplayed && shufIndexes.size() >= LINES_BEFORE_ESTIMATING_MEMORY_OVERHEAD)
        {
            memoryOverheadDisplayed = true;
            double averageBytesPerLine = (double)i / (double)shufIndexes.size();
            double memoryOverhead = sizeof(ll) / averageBytesPerLine + 1;
            fprintf(stderr, "mean line-length is %.2f, estimated memory usage is %.2f * %.2f GB = %.2f GB\nTip: If you would like use exactly %.2f GB of memory, use MEMORY=%.4f ./terashuf ...\n", averageBytesPerLine - 1, memoryOverhead, memory, memoryOverhead * memory, memory, memory / memoryOverhead);
        }
    }
    return bufPos;
}

int main()
{
    char const *sepStr = std::getenv("SEP");
    if (sepStr != NULL && strlen(sepStr))
        sep = sepStr[0];

    char *seedStr = std::getenv("SEED");
    if (seedStr != NULL && strlen(seedStr))
        seed = strtol(seedStr, NULL, 10);
    srand(seed);

    char *skipStr = std::getenv("SKIP");
    int skipLines = 0;
    if (skipStr != NULL && strlen(skipStr))
        skipLines = strtol(skipStr, NULL, 10);

    char const *tmpDir = std::getenv("TMPDIR");
    if (tmpDir == NULL)
        tmpDir = "/tmp";
    char *tmpNameTemplate = (char *)malloc((strlen(tmpDir) + sizeof(TMP_NAME_TEMPLATE)) * sizeof(char));
    strcpy(tmpNameTemplate, tmpDir);
    strcat(tmpNameTemplate, TMP_NAME_TEMPLATE);

    char *memoryStr = std::getenv("MEMORY");
    if (memoryStr != NULL && strlen(memoryStr))
        memory = strtof(memoryStr, NULL);

    bufBytes = sizeof(char) * (ll)(memory * 1024. * 1024. * 1024.);
    buf = (char *)malloc(bufBytes);
    fprintf(stderr, "trying to allocate %zu bytes\n", bufBytes);
    if (buf == NULL)
    {
        fprintf(stderr, "failed to allocate buf memory\n");
        return 1;
    }

    fprintf(stderr, "\nstarting read\n");

    ll totalBytesRead = 0, totalLinesRead = 0;

    std::vector<TmpFile *> files;

    while (skipLines)
    {
        char c = fgetc(stdin);
        if (c == EOF)
            return 0;
        if (fputc(c, stdout) == EOF)
        {
            fprintf(stderr, "failed to write SKIP lines. is disk full?");
            return 1;
        }
        if (c == sep)
            skipLines--;
    }

    while (fillBufAndMarkLines(stdin))
    {
        TmpFile *tmpFile = (TmpFile *)malloc(sizeof(TmpFile));
        FILE *f;
        if (bufPos < bufBytes && files.size() == 0)
        {
            // finished reading input using single buffer, don't need to create
            // tmpfile, will flush buffer to stdout directly and exit
            f = stdout;
        }
        else
        {
            // create temp file, will flush buffer to this temp file
            // and continue loop if there is more data in stdin
            tmpFile->path = (char *)malloc((strlen(tmpNameTemplate) + 1) * sizeof(char));
            strcpy(tmpFile->path, tmpNameTemplate);
            int fd = mkstemp(tmpFile->path);
            if (fd == -1)
            {
                fprintf(stderr, "failed to create fd tmp file %s\n",
                        tmpNameTemplate);
                return 1;
            }
            tmpFile->f = fdopen(fd, "wb+");
            if (tmpFile->f == NULL)
            {
                fprintf(stderr, "failed to create tmp file %s\n",
                        tmpFile->path);
                return 1;
            }
            f = tmpFile->f;
        }
        totalBytesRead += shufFlushBuf(f);
        totalLinesRead += shufIndexes.size();
        tmpFile->lines = shufIndexes.size();
        fprintf(stderr, "\rlines read: %zu, gb read: %zu", totalLinesRead,
                totalBytesRead / (1024 * 1024 * 1024));
        files.push_back(tmpFile);
    }

    // inform totalLines and totalBytesRead
    fprintf(stderr, "\nRead %zu lines, %zu bytes, have %d tmp files\n",
            totalLinesRead, totalBytesRead, (int)files.size());

    if (files.size() == 1)
    {
        fprintf(
            stderr,
            "\nShuffled directly to stdout without using tmpfiles, done!\n");
        return 0;
    }

    std::vector<TmpFile *> files2;

    for (std::vector<TmpFile *>::const_iterator it = files.begin(); it != files.end(); ++it)
    {
        TmpFile *file = *it;
        rewind(file->f);
        // for bufferedFgetc
        file->buf = (char *)malloc(IO_CHUNK);
        file->bufPos = 0;
        file->bufN = 0;
        file->eof = false;
    }

    ll totalBytesWritten = 0, linesRemaining = totalLinesRead, totalBytesWrittenForProgress = 0;

    fprintf(stderr, "\nstarting write to output\n");

    while (linesRemaining)
    {
        ll randLine = std::rand() % linesRemaining;
        ll cumSum = 0;
        for (std::vector<TmpFile *>::const_iterator it = files.begin(); it != files.end(); ++it)
        {
            TmpFile *file = *it;
            cumSum += file->lines;
            if (randLine < cumSum)
            {
                linesRemaining--;
                file->lines--;
                ll bytesRead = readLine(file);
                if (!file->lines)
                {
                    fclose(file->f);
                    unlink(file->path);
                }
                ll bytesWritten = fwrite(buf, sizeof(char), bytesRead, stdout);
                if (bytesRead != bytesWritten)
                {
                    fprintf(stderr, "\nFATAL ERROR: failed to write line to disk. is there space left?\n");
                    return 1;
                }
                totalBytesWritten += bytesWritten;
                totalBytesWrittenForProgress += bytesWritten;
                break;
            }
        }
        // progress report
        if (!linesRemaining || totalBytesWrittenForProgress >= bufBytes)
        {
            fprintf(stderr, "\rlines written: %zu, gb written: %zu",
                    totalLinesRead - linesRemaining, totalBytesWritten / (1024 * 1024 * 1024));
            totalBytesWrittenForProgress = 0;
        }
    }
    fprintf(stderr, "\ndone\n");

    return 0;
}
