# RLB - Efficient Compression and Retrieval Algorithm

## Overview

RLB (Run-Length and BWT) is a high-efficiency compression and retrieval algorithm, leveraging the principles of the Burrows-Wheeler Transform (BWT) combined with Run-Length Encoding (RLE). This project focuses on providing a robust solution for compressing text files and facilitating efficient keyword-based retrieval of records without the need for full decompression.

## Features

- **Efficient Compression**: Utilizes BWT and RLE to compress text files into a compact `.rlb` format.
- **Selective Retrieval**: Allows for keyword-based searches within compressed files, enabling the retrieval of specific records without complete decompression.
- **Record-Based Structure**: Each `[xxx]` in the text file is treated as an individual record, making it well-suited for datasets with delineated entries.

## Usage

### Compression

The RLB project can compress text files of the following format:

```txt
[8]Computers in industry[9]Data compression[10]Integration[11]Big data indexing
```

Each `[xxx]` is recognized as a separate record. The compression process outputs a `.rlb` file, which contains the compressed data.

### Retrieval

RLB supports keyword-based searches within the compressed `.rlb` files. This feature allows users to efficiently search for and retrieve specific records based on keywords, without the need to decompress the entire file.
