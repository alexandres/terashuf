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
#include <random>
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

ll shufFlushBuf(char *buf, std::vector<ll> &shufIndexes, std::mt19937_64 &rng, char sep, FILE *f)
{
    std::shuffle(shufIndexes.begin(), shufIndexes.end(), rng);
    ll bytesWritten = 0;
    for (auto line : shufIndexes)
    {
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

char bufferedFgetc(TmpFile &f)
{
    if (!f.eof && f.bufPos == f.bufN)
    {
        f.bufN = fread(f.buf, sizeof(char), IO_CHUNK, f.f);
        f.bufPos = 0;
        if (!f.bufN)
            f.eof = true;
    }
    if (f.eof)
        return EOF;
    return f.buf[f.bufPos++];
}

ll readLine(char *buf, char sep, TmpFile &f)
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

bool fillBufAndMarkLines(FILE *f, char *buf, const ll bufBytes, std::vector<ll> &shufIndexes, char sep, float memory)
{
    static ll bufPos = 0, lastLineEndPos = 0;
    static bool memoryOverheadDisplayed;

    // see if anything left from previous fillBuf
    if (bufPos)
    {
        ll bytesToCpy = bufBytes - lastLineEndPos;
        memmove(buf, buf + lastLineEndPos, bytesToCpy);
        bufPos = bytesToCpy;
    }

    bufPos += fread(buf + bufPos, sizeof(char), bufBytes - bufPos, f);
    // if got nothing in buffer, signal we're done
    if (!bufPos)
        return true;

    // handle missing sep on last line
    // buffer not full and previous position is not newline
    if (bufPos < bufBytes && buf[bufPos - 1] != sep)
        buf[bufPos++] = sep;

    // mark lines and store pos in shufIndexes
    ll lineStart = 0;
    shufIndexes.clear();
    lastLineEndPos = 0;
    for (ll i = 0; i < bufPos; i++)
    {
        if (buf[i] != sep)
            continue;
        shufIndexes.push_back(lineStart);
        lineStart = i + 1;
        lastLineEndPos = i + 1;
        if (!memoryOverheadDisplayed && shufIndexes.size() >= LINES_BEFORE_ESTIMATING_MEMORY_OVERHEAD)
        {
            memoryOverheadDisplayed = true;
            double averageBytesPerLine = (double)i / (double)shufIndexes.size();
            double memoryOverhead = sizeof(ll) / averageBytesPerLine + 1;
            fprintf(stderr, "mean line-length is %.2f, estimated memory usage is %.2f * %.2f GB = %.2f GB\nTip: If you would like use exactly %.2f GB of memory, use MEMORY=%.4f ./terashuf ...\n", averageBytesPerLine - 1, memoryOverhead, memory, memoryOverhead * memory, memory, memory / memoryOverhead);
        }
    }
    if (shufIndexes.size() == 0)
    {
        fprintf(
            stderr,
            "\nFATAL ERROR: line too long to fit in buffer (> %zu bytes):\n",
            bufBytes);
        fwrite(buf, sizeof(char), std::min(bufBytes, (ll)50), stderr);
        fprintf(stderr, "...\n");
        exit(1); // line was too long to fit in buf, can't be shuffled
    }
    // if buffer isn't full, signal we're done
    return bufPos < bufBytes;
}

// see https://en.wikipedia.org/wiki/Fenwick_tree and
// and http://timvieira.github.io/blog/post/2016/11/21/heaps-for-incremental-computation/
// ported from https://github.com/timvieira/arsenal/blob/master/arsenal/datastructures/heap/sumheap.pyx
class FenwickTree
{
private:
    std::vector<ll> S;
    int n, d;

    void heapify(const std::vector<ll> &w)
    {
        // Create sumheap from weights `w` in O(n) time.
        for (int i = 0; i < n; i++)
            S[d + i] = w[i]; // store `w` at leaves.
        for (int i = d - 1; i > 0; i--)
            S[i] = S[2 * i] + S[2 * i + 1];
    }

public:
    FenwickTree(const std::vector<ll> &w)
    {
        n = w.size();
        d = int(std::pow(2, std::ceil(std::log2(n)))); // number of intermediates
        S = std::vector<ll>(2 * d, 0);                 // intermediates + leaves
        heapify(w);
    }

    ll getCountAtIndex(int k)
    {
        return S[d + k];
    }

    int findIndexAndDraw(ll p)
    {
        // Draw from from sumheap, O(log n) per draw.
        // Use binary search to find the index of the largest cumsum (represented as a
        // heap) value that is less than the probe p.
        ll i = 1;
        while (true)
        {
            S[i]--; // draw
            if (i >= d)
                break;
            // Determine if the value is in the left or right subtree.
            i *= 2;         // Point at left child
            ll left = S[i]; // Cumsum under left subtree.
            if (p > left)
            {              // Value is in right subtree.
                p -= left; // Subtract cumsum from left subtree
                i++;       // Point at right child
            }
        }
        return i - d; // return index after subtracting internal nodes
    }
};

int main()
{
    char const *sepStr = std::getenv("SEP");
    const char sep = (sepStr != NULL && strlen(sepStr)) ? sepStr[0] : '\n';

    char *seedStr = std::getenv("SEED");
    const int seed = (seedStr != NULL && strlen(seedStr)) ? strtol(seedStr, NULL, 10) : time(NULL);
    std::mt19937_64 rng(seed);

    char *skipStr = std::getenv("SKIP");
    const int skipLines = (skipStr != NULL && strlen(skipStr)) ? strtol(skipStr, NULL, 10) : 0;

    char const *tmpDir = std::getenv("TMPDIR");
    if (tmpDir == NULL)
        tmpDir = "/tmp";
    char *tmpNameTemplate = (char *)malloc((strlen(tmpDir) + sizeof(TMP_NAME_TEMPLATE)) * sizeof(char));
    strcpy(tmpNameTemplate, tmpDir);
    strcat(tmpNameTemplate, TMP_NAME_TEMPLATE);

    char *memoryStr = std::getenv("MEMORY");
    const float memory = (memoryStr != NULL && strlen(memoryStr)) ? strtof(memoryStr, NULL) : 4;

    std::vector<ll> shufIndexes;

    const ll bufBytes = sizeof(char) * (ll)(memory * 1024. * 1024. * 1024.);
    char *buf = (char *)malloc(bufBytes);
    fprintf(stderr, "trying to allocate %zu bytes\n", bufBytes);
    if (buf == NULL)
    {
        fprintf(stderr, "failed to allocate buf memory\n");
        return 1;
    }

    fprintf(stderr, "\nstarting read\n");

    for (int i = 0; i < skipLines;)
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
            i++;
    }
    fprintf(stderr, "skipped %d lines\n", skipLines);

    ll totalBytesRead = 0, totalLinesRead = 0;
    std::vector<TmpFile> files;
    bool reachedEof = false;
    while (!reachedEof)
    {
        reachedEof = fillBufAndMarkLines(stdin, buf, bufBytes, shufIndexes, sep, memory);
        TmpFile tmpFile;
        // default output is stdout. if input fit in buffer, no need to use
        // temp files, just write to stdout.
        tmpFile.f = stdout;
        if (!reachedEof || files.size() > 0)
        {
            // haven't reached eof so have to call fillBuf again.
            // or we did did reach eof but there is already a temp file,
            // so we are already in temp file mode, not direct to stdout.
            tmpFile.path = (char *)malloc((strlen(tmpNameTemplate) + 1) * sizeof(char));
            strcpy(tmpFile.path, tmpNameTemplate);
            int fd = mkstemp(tmpFile.path);
            if (fd == -1)
            {
                fprintf(stderr, "failed to create fd tmp file %s\n",
                        tmpNameTemplate);
                return 1;
            }
            tmpFile.f = fdopen(fd, "wb+");
            if (tmpFile.f == NULL)
            {
                fprintf(stderr, "failed to create tmp file %s\n",
                        tmpFile.path);
                return 1;
            }
        }
        totalBytesRead += shufFlushBuf(buf, shufIndexes, rng, sep, tmpFile.f);
        totalLinesRead += shufIndexes.size();
        tmpFile.lines = shufIndexes.size();
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

    std::vector<ll> linesRemainingPerFile;

    for (auto &file : files)
    {
        rewind(file.f);
        // for bufferedFgetc
        file.buf = (char *)malloc(IO_CHUNK);
        file.bufPos = 0;
        file.bufN = 0;
        file.eof = false;
        linesRemainingPerFile.push_back(file.lines);
    }

    auto fenwickTree = FenwickTree(linesRemainingPerFile);

    ll totalBytesWritten = 0, linesRemaining = totalLinesRead, totalBytesWrittenForProgress = 0;

    fprintf(stderr, "\nstarting write to output\n");

    while (linesRemaining)
    {
        ll randLine = std::uniform_int_distribution<ll>{0, linesRemaining - 1}(rng);
        auto fileIdx = fenwickTree.findIndexAndDraw(randLine + 1);
        auto linesRemainingInFile = fenwickTree.getCountAtIndex(fileIdx);
        linesRemaining--;
        TmpFile &file = files[fileIdx];
        ll bytesRead = readLine(buf, sep, file);
        if (!linesRemainingInFile)
        {
            fclose(file.f);
            unlink(file.path);
        }
        ll bytesWritten = fwrite(buf, sizeof(char), bytesRead, stdout);
        if (bytesRead != bytesWritten)
        {
            fprintf(stderr, "\nFATAL ERROR: failed to write line to disk. is there space left?\n");
            return 1;
        }
        totalBytesWritten += bytesWritten;
        totalBytesWrittenForProgress += bytesWritten;
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