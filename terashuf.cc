// Copyright (c) 2017 Salle, Alexandre <atsalle@inf.ufrgs.br>
// Author: Salle, Alexandre <atsalle@inf.ufrgs.br>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "string.h"
#include "unistd.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

typedef long long ll;

const int MAX_PATH_LEN = 1000;
const int IO_CHUNK = 64 * 1024;

ll maxLineLen;
ll bufBytes;
ll bufPos;
std::vector<ll> shufIndexes;
char* buf;
ll longestLine;


unsigned long long llrand() // 64-bit rand from of https://stackoverflow.com/a/28116032/67550
{
    unsigned long long r = 0;

    for (int i = 0; i < 5; ++i) {
	r = (r << 15) | (rand() & 0x7FFF);
    }

    return r & 0xFFFFFFFFFFFFFFFFULL;
}

ll shufFlushBuf(FILE* f)
{
    ll bytesWritten = 0;
    for (ll i = shufIndexes.size() - 1; i >= 0; --i) {
	if (i > 0)
	    std::swap(shufIndexes[i], shufIndexes[llrand() % (i + 1)]);
	ll line = shufIndexes[i];
	ll j = 0;
	while (*(buf + line + j) != '\n')
	    j++;
	ll bytesToWrite = j + 1;
	bytesWritten += bytesToWrite;
	for (ll k = 0; k < bytesToWrite;) {
	    ll chunkSize = MIN(bytesToWrite - k, IO_CHUNK);
	    fwrite(buf + line + k, sizeof(char), chunkSize, f);
	    k += chunkSize;
	}
    }
    return bytesWritten;
}

ll readLine(char* buf, FILE* f)
{
    ll initialBufPos = bufPos, c = 0;
    while ((c = fgetc(f)) != EOF) {
	buf[bufPos++] = c;
	if (c == '\n') {
	    shufIndexes.push_back(initialBufPos);
	    return bufPos - initialBufPos;
	}
    }
    return 0;
}

ll fillBufAndMarkLines(char* buf, FILE* f)
{
    // anything remaining from last fillBuf? move to buf start
    if (bufPos == bufBytes) {
	// check if buf ends in newline, otherwise keep track of where incomplete line started so it can be copied to beginning of buffer after flush
	while (bufPos > 0 && buf[bufPos - 1] != '\n')
	    bufPos--;
	if (bufPos == 0) {
	    fprintf(stderr, "FATAL ERROR: line too long to fit in buffer (> %lld bytes):\n", bufBytes);
	    fwrite(buf, sizeof(char), MIN(bufBytes, 50), stderr);
	    std::cerr << "...\n";
	    exit(1); // line was too long to fit in buf, can't be shuffled
	}
	ll bytesToCpy = bufBytes - bufPos;
	memcpy(buf, buf + bufPos, bytesToCpy);
	bufPos = bytesToCpy;
    } else {
	bufPos = 0;
    }

    // fill entire buf
    ll bytesRead = 0;
    while ((bytesRead = fread(buf + bufPos, sizeof(char), MIN(IO_CHUNK, bufBytes - bufPos), f))) {
	bufPos += bytesRead;
	if (bufPos == bufBytes)
	    break;
    }

    shufIndexes.clear();
    // mark lines and store pos in shufIndexes
    ll lineStart = 0;
    for (ll i = 0; i < bufPos; i++) {
	if (buf[i] == '\n') {
	    shufIndexes.push_back(lineStart);
	    ll lineLen = i - lineStart + 1;
	    longestLine = MAX(longestLine, lineLen);
	    lineStart = i + 1;
	}
    }
    return bufPos;
}

struct TmpFile {
    FILE* f;
    char* path;
};

int main()
{
    srand(time(NULL));

    char const* tmpDir = std::getenv("TMPDIR");
    if (tmpDir == NULL)
	tmpDir = "/tmp";
    char tmpNameTemplate[MAX_PATH_LEN];
    strcpy(tmpNameTemplate, tmpDir);
    strcat(tmpNameTemplate, "/terashufXXXXXX");

    char* memoryStr = std::getenv("MEMORY");
    double memory = 4.;
    if (memoryStr != NULL)
	memory = std::stof(std::string(memoryStr));

    bufBytes = sizeof(char) * (ll)(memory * 1024. * 1024. * 1024.);
    buf = (char*)malloc(bufBytes);
    fprintf(stderr, "trying to allocate %lld bytes\n", bufBytes);
    if (buf == NULL) {
	fprintf(stderr, "failed to allocate buf memory\n");
	return -1;
    }

    fprintf(stderr, "\nstarting read\n");

    std::vector<TmpFile*> files;

    ll newBufPos = 0;
    ll totalBytesRead = 0, totalLinesRead = 0;

    while ((newBufPos = fillBufAndMarkLines(buf, stdin))) {
	TmpFile* tmpFile = (TmpFile*)malloc(sizeof(TmpFile));
	FILE* f;
	if (newBufPos < bufBytes && files.size() == 0) {
	    // finished reading input using single buffer, don't need to create tmpfile, flush buffer to stdout directly
	    f = stdout;
	} else {
	    tmpFile->path = (char*)malloc(MAX_PATH_LEN);
	    strcpy(tmpFile->path, tmpNameTemplate);
	    int fd = mkstemp(tmpFile->path);
	    if (fd == -1) {
		fprintf(stderr, "failed to create fd tmp file %s\n", tmpNameTemplate);
		return -1;
	    }
	    tmpFile->f = fdopen(fd, "wb+");
	    if (tmpFile->f == NULL) {
		fprintf(stderr, "failed to create tmp file %s\n", tmpFile->path);
		return -1;
	    }
	    f = tmpFile->f;
	}
	if (shufIndexes.size() > 0)
	    totalBytesRead += shufFlushBuf(f);
	totalLinesRead += shufIndexes.size();
	fprintf(stderr, "\rlines read: %lld, gb read: %lld", totalLinesRead, totalBytesRead / (1024 * 1024 * 1024));
	files.push_back(tmpFile);
    }

    // inform totalLines and totalBytesRead
    fprintf(stderr, "\nRead %lld lines, %lld bytes, have %d tmp files\n", totalLinesRead, totalBytesRead, (int)files.size());

    if (files.size() == 1) {
	fprintf(stderr, "\nShuffled directly to stdout without using tmpfiles, done!\n");
	return 0;
    }

    double averageBytesPerLine = (double)totalBytesRead / (double)totalLinesRead;
    ll linesPerFile = (ll)((double)bufBytes / averageBytesPerLine);
    ll shuffleChunkPerFile = linesPerFile / files.size();

    std::vector<TmpFile*> files2;

    for (int i = 0; i < (int)files.size(); i++) {
	rewind(files[i]->f);
    }

    ll totalBytesWritten = 0, totalLinesWritten = 0;

    fprintf(stderr, "\nstarting write to output\n");

    while (files.size() > 0) {
	bufPos = 0;
	for (int i = 0; i < (int)files.size(); i++) {
	    bool keepFile = true;
	    for (ll j = 0; j < shuffleChunkPerFile; j++) {
		// check if enough room in buffer to hold longest line
		if (bufBytes - bufPos < longestLine)
		    break;
		ll bytesRead = readLine(buf, files[i]->f);
		if (bytesRead == 0) {
		    keepFile = false;
		    break;
		}
	    }
	    if (keepFile)
		files2.push_back(files[i]);
	    else {
		fclose(files[i]->f);
		unlink(files[i]->path);
	    }
	}
	totalBytesWritten += shufFlushBuf(stdout);
	totalLinesWritten += shufIndexes.size();
	fprintf(stderr, "\rlines written: %lld, gb written: %lld", totalLinesWritten, totalBytesWritten / (1024 * 1024 * 1024));
	shufIndexes.clear();
	std::swap(files, files2);
	files2.clear();
    }
    fprintf(stderr, "\ndone\n");

    return 0;
}
