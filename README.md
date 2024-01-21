# RLB - Efficient Compression and Retrieval Algorithm

## Overview

RLB (Run-Length and BWT) is a high-efficiency compression and retrieval algorithm, leveraging the principles of the Burrows-Wheeler Transform (BWT) combined with Run-Length Encoding (RLE). This project focuses on providing a robust solution for compressing text files and facilitating efficient keyword-based retrieval of records without the need for full decompression.

## Features

- **Efficient Compression**: Utilizes BWT and RLE to compress text files into a compact `.rlb` format.
- **Selective Retrieval**: Allows for keyword-based searches within compressed files, enabling the retrieval of specific records without complete decompression.
- **Record-Based Structure**: Each `[xxx]` in the text file is treated as an individual record, making it well-suited for datasets with delineated entries.

### Compression

The RLB project can compress text files of the following format:

```txt
[8]Computers in industry[9]Data compression[10]Integration[11]Big data indexing
```

Each `[xxx]` is recognized as a separate record. The compression process outputs a `.rlb` file, which contains the compressed data.

### Retrieval

RLB supports keyword-based searches within the compressed `.rlb` files. This feature allows users to efficiently search for and retrieve specific records based on keywords, without the need to decompress the entire file.



## Usage

This project includes a script `example.sh` that demonstrates basic usage to compress and search in `example/dummy.txt`. 

**Compress**

```bash
$ ./bin/compress <input txt file> <output rlb file>
```

e.g. 

```bash
$ ./bin/compress example/dummy.txt example/dummy.rlb
```

**Search**

```bash
$ ./bin/search <compressed rlb file> <index file> <keyword to search>
```

e.g.

```bash
$ ./bin/search example/dummy.rlb example/dummy.idx "in"
```



**Using the .idx File for Accelerated Search**

Our search program leverages an index file (with the extension `.idx`) to enhance and accelerate the search process. Here’s how it works:

- When the .idx File Exists: If an `.idx` file is already present at the specified path, our search program will utilize this index file to speed up the search operation. The `.idx` file contains pre-processed data that allows for quicker lookup and retrieval, thus significantly improving the efficiency of the search process.
- When the .idx File Does Not Exist: In cases where an `.idx` file is not found at the given path, our search program has the capability to generate and store data that facilitates faster searching in future operations. During the initial search without an `.idx` file, the program may take longer to complete the search; however, it simultaneously creates and saves necessary data in a new `.idx` file at the specified path. This file will then be used in subsequent searches to achieve faster performance.
