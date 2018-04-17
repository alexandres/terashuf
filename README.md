# terashuf

terashuf implements a quasi-shuffle algorithm for shuffling multi-terabyte text files using limited memory. It is a C++ implementation of [this Python script](https://github.com/alexandres/lexvec/blob/master/shuffle.py). 

## Why not GNU sort -R instead?

terashuf has 2 advantages over `sort -R`:

1. terashuf is much, much faster. See benchmark below.
2. It can shuffle duplicate lines. To deal with duplicate lines in `sort`, the input has to be modified (append an incremental token) so that duplicate lines are different otherwise sort will hash them to the same value and place them in adjacent lines, which is not desirable in a shuffle! Then the tokens have to be removed. It's simpler to use terashuf where none of this is required. 

## Why not GNU shuf?

`shuf` does all the shuffling in-memory, which is a no-go for files larger than memory.

For small files, terashuf doesn't write any temporary files and so functions exactly like `shuf`. In 
the benchmark below, terashuf marginally outperforms shuf.

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

It reads 2 ENV variables:

- TMPDIR: defaults to /tmp if not set.
- MEMORY: defaults to 4.0, meaning use a shuffle buffer of 4 GB. Set this as high as your machine allows.

**Note: the last line in the file to be shuffled will be ignored if it does not end with a newline marker (\n).**

When shuffling very large files, terashuf needs to keep open `SIZE_OF_FILE_TO_SHUFFLE / MEMORY` temporary files. **Make sure to [set the maximum number of file descriptors](https://www.cyberciti.biz/faq/linux-increase-the-maximum-number-of-open-files/) to at least this number.** By setting a large file descriptor limit, you ensure that terashuf won't abort a shuffle midway, saving precious researcher time. 

## Quasi-shuffle

terashuf implements a quasi-shuffle as follows:

1. Divide N input lines into K files containing L lines.
2. Shuffle each of the K files (this is done in memory before writing the file).
3. Read one line from each of the K files into a buffer until the buffer has L lines.
4. Shuffle the buffer and write to output.
5. Repeat 3. and 4. until all lines have been written to output.

## TODO

Pull requests are very welcome!

- [x] Rather than use fixed-length lines in the buffer which wastes memory, use variable-length lines such that all buffer memory is used.
- [ ] Implement --help
- [ ] Implement `shuf` interface so that terashuf becomes a drop-in replacement
- [x] Add benchmarks

# License

Copyright (c) 2017 Salle, Alexandre <atsalle@inf.ufrgs.br>. All work in this package is distributed under the MIT License.
