# terashuf

terashuf shuffles multi-terabyte text files using limited memory. It is an improved (fair shuffling!) C++ implementation of [this Python script](https://github.com/alexandres/lexvec/blob/a3e894b5ebf8fb292fc0d1d7b10b8f82e2ac3392/shuffle.py). 

## Why not GNU sort -R instead?

terashuf has 2 advantages over `sort -R`:

1. terashuf is much, much faster. See benchmark below.
2. It can shuffle duplicate lines. To deal with duplicate lines in `sort`, the input has to be modified (append an incremental token) so that duplicate lines are different otherwise sort will hash them to the same value and place them in adjacent lines, which is not desirable in a shuffle! Then the tokens have to be removed. It's simpler to use terashuf where none of this is required. 

## Why not GNU shuf?

`shuf` does all the shuffling in-memory, which is a no-go for files larger than memory.

For small files, terashuf doesn't write any temporary files and so functions exactly like `shuf`.

## Benchmark

The following compares shuffle times for a 20GB Wikipedia dump. terashuf is tested with limited memory
and with memory large enough to fit the entire file (in-memory like shuf): 

| Command        | Memory (GB)     | Real Time           | User Time   |  Sys Time    |
|----------------|-----------------|---------------------|-------------|--------------|
| terashuf       | 4               | 6m16s               | 4m53s       |  44s         |
| terashuf       | 20              | 2m32s               | 1m17s       |  21s         |
| shuf           | 20              | 2m46s               | 50s         |  27s         |
| sort -R        | 4               | 170m51s             | 649m7s      |  52s         |  

*Benchmark run on Xeon E5-2630 @ 2.60GHz with 128GB of RAM.*

*Note: I'm looking for pull requests implementing the shuf interface so that terashuf can become a drop-in
replacement for shuf.*

## Build

terashuf can be built by calling ```$ make```. It has no dependencies other than the stdlib.

## Usage

`$ ./terashuf < filetoshuffle.txt > shuffled.txt`

It reads 4 ENV variables:

- TMPDIR (string): where to store temporary files; defaults to /tmp if not set.
- MEMORY (float): how much memory to use in GB for buffers; defaults to 4.0.
- SKIP (int): how many lines to skip at beginning of input; defaults to 0. When shuffling CSV files, set to 1 to preserve header.
- SEP (character): character to use as line separator; defaults to `$'\n'`.
- SEED (int): number to seed RNG; defaults to current time. Set this *and* MEMORY to fixed values for deterministic shuffles.

**When shuffling very large files, terashuf needs to keep open `SIZE_OF_FILE_TO_SHUFFLE (in GB) / MEMORY (in GB)` temporary files.**

For example, to shuffle a 5TB file using 4GB (the default) of memory you need 1250 file descriptors. 
On most Linux distributions, the default number of files a single process can have open simultaneously is 1024, so the shuffle would fail.

**Setting a large limit by running `$ ulimit -n 100000` beforehand, you ensure that terashuf won't abort a shuffle midway, saving precious researcher time.**

Read more about [setting the maximum number of file descriptors](https://www.cyberciti.biz/faq/linux-increase-the-maximum-number-of-open-files/).

## Shuffle

terashuf shuffles as follows:

1. Divide N input lines into K files containing L lines.
2. Shuffle each of the K files (this is done in memory before writing the file).
3. Sample one of the K files where the probability of drawing a file is proportional to the number of lines remaining in the file.
4. Pop the first line from the sampled file and write it to output.
5. Repeat 3-4 until all lines have been written to output.

This algorithm was suggested by Nick Downing and proven fair by Ivan in this [thread](https://lemire.me/blog/2010/03/15/external-memory-shuffling-in-linear-time/). The previous version of terashuf used a quasi(unfair)-shuffle. 

## TODO

Pull requests are very welcome!

- [x] Rather than use fixed-length lines in the buffer which wastes memory, use variable-length lines such that all buffer memory is used.
- [ ] Implement --help
- [ ] Implement `shuf` interface so that terashuf becomes a drop-in replacement
- [x] Add benchmarks

# License

Copyright (c) 2017 Salle, Alexandre <atsalle@inf.ufrgs.br>. All work in this package is distributed under the MIT License.
