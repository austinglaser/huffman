Huffman Encoding
========

A program that encodes plaintext files in a compressed filetype
using huffman encoding. I've been unnoficially denoting this filetype
with a .huf file extension.

Compiling and Running
========

In ./cpp, run ```make huffman_encode```.
Then, run ```./huffman_encode <-e | -d> in_file out_file```

* -e denotes encode mode, -d decode mode
* out_file will be overwritten
* decoding will only work on files created with this utility (enforced
  with magic number)
