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

#include <cstdlib>
#include <string>
#include <vector>
#include "string.h"
#include "unistd.h"

const int MAX_PATH_LEN = 1000;

int maxLineLen;
long long bufBytes;
long long *shufIndexes;
char *buf;

void shufFlushBuf(long long lines, FILE* f) {
    for (long long i = 0; i < lines; i++) {
        shufIndexes[i] = i; 
    }
    for (long long i = lines - 1; i >= 0; --i) {
        if (i > 0) std::swap(shufIndexes[i],shufIndexes[rand() % (i+1)]);
        long long line = shufIndexes[i];
        int j = 0;
        while (*(buf+line*maxLineLen+j) != '\n') j++;
        fwrite(buf+line*maxLineLen, sizeof(char), j + 1, f);
    }
}

int readLine(char *buf, FILE* f) {
    int i = 0, c = 0;
    while ((c = fgetc(f)) != EOF && i < maxLineLen) {
        buf[i] = c;
        i++;
        if (c == '\n') return i;
    }
    if (c == EOF) return 0; // we're discarding last line if no line break at the end

    // line is too long, consume till the end of the line or eof
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') return -1;
    }
    return 0;
} 

struct TmpFile {
    FILE* f;
    char* path;
};

int main() {
    srand(time(NULL));
    
    char const *tmpDir = std::getenv("TMPDIR");
    if (tmpDir == NULL) tmpDir = "/tmp";  
    char tmpNameTemplate[MAX_PATH_LEN];
    strcpy(tmpNameTemplate, tmpDir);
    strcat(tmpNameTemplate, "/terashufXXXXXX");

    char *memoryStr = std::getenv("MEMORY");
    double memory = 4.;
    if (memoryStr != NULL) memory = std::stof(std::string(memoryStr));

    char *maxLineLenStr = std::getenv("MAXLINELEN");
    maxLineLen = 1000;
    if (maxLineLenStr != NULL) maxLineLen = std::stoi(std::string(maxLineLenStr));
    
    long long bufLines = memory * (1024*1024*1024) / (sizeof(char)*maxLineLen);
    bufBytes = sizeof(char)*bufLines*maxLineLen;
    long long shufIndexesBytes = sizeof(long long)*bufLines;
    buf = (char*) malloc(bufBytes);
    shufIndexes = (long long *) malloc(shufIndexesBytes);
    fprintf(stderr, "trying to allocate %lld bytes\n", bufBytes + shufIndexesBytes);
    if (buf == NULL || shufIndexes == NULL) {
        fprintf(stderr, "failed to allocate memory\n");
        return -1;
    }

    fprintf(stderr, "\nstarting read\n");

    std::vector<TmpFile*> files;

    int bytesRead = 0;
    long long lines = 0, totalBytesRead = 0, totalLinesRead = 0, longLines = 0;

    while (true) {
        bytesRead = readLine(buf+lines*maxLineLen, stdin);
        // check for error in bytesRead
        if (bytesRead == -1) {
            // exceeded maxLineLen, either warn on stderr or panic
            longLines++;
        } else {
            if (bytesRead > 0) {
                lines++;
                totalLinesRead++;
                totalBytesRead += bytesRead;
                if (totalLinesRead % 1000000 == 0) fprintf(stderr, "\rlines read: %lld, gb read: %lld", totalLinesRead, totalBytesRead / (1024*1024*1024));                
            }
            // check if buffer is full or buffer part full and input finished, if so shuf and flush
            if ((bytesRead == 0 && lines > 0) || lines == bufLines) {
                TmpFile *tmpFile = (TmpFile*) malloc(sizeof(TmpFile));
                FILE* f;
                if (bytesRead == 0 && files.size() == 0) {
                    // finished reading input using single buffer, don't need to create tmpfile, flush buffer to stdout directly
                    f = stdout;
                } else {
                    tmpFile->path = (char*) malloc(MAX_PATH_LEN);
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
                shufFlushBuf(lines, f);
                files.push_back(tmpFile);
                lines = 0;
            }
            if (bytesRead == 0) break;
        }
    }
    
    // inform totalLines and totalBytesRead 
    fprintf(stderr, "\nRead %lld lines, %lld bytes, have %d tmp files, skipped %lld long lines\n", totalLinesRead, totalBytesRead, (int) files.size(), longLines);

    if (files.size() == 1) {
        fprintf(stderr, "\nShuffled directly to stdout without using tmpfiles, done!\n");
        return 0;
    }

    long long linesPerFile = bufLines / files.size();
     
    std::vector<TmpFile*> files2;

    for (int i = 0; i < (int) files.size(); i++) {
            rewind(files[i]->f);
    }

    long long totalBytesWritten = 0, totalLinesWritten = 0;

    fprintf(stderr, "\nstarting write to output\n");

    while (files.size() > 0) {
        lines = 0;
        for (int i = 0; i < (int) files.size(); i++) {
            bool keepFile = true;
            for (int j = 0; j < (int) linesPerFile; j++) {
                int bytesRead = readLine(buf+lines*maxLineLen, files[i]->f);
                if (bytesRead > 0) {
                    lines++;
                    totalLinesWritten++;
                    totalBytesWritten += bytesRead;
                    if (totalLinesWritten % 1000000 == 0) fprintf(stderr, "\rlines written: %lld, gb written: %lld", totalLinesWritten, totalBytesWritten / (1024*1024*1024));
                } else {
                    keepFile = false;
                    break;
                }
            }
            if (keepFile) files2.push_back(files[i]);
            else {
                fclose(files[i]->f);
                unlink(files[i]->path);
            }
        }
        if (lines > 0) shufFlushBuf(lines, stdout);
        std::swap(files, files2);
        files2.clear();
    }
    fprintf(stderr, "\ndone\n");

    return 0;
}