>[!WARNING]
>This project is under development and stability is not guaranteed.

# Introduce

a simple lrc parsing library

# Install

1. add this repository as git submodule
``` bash
git submodule add <this repo> <your path>
```
1. Add the following statement to your CMakeLists.txt
``` cmake
add_subdirectory(lrc-parser)
target_link_libraries(your_app PRIVATE lrc_parser)
```

# Usage

Input, output, that's all.

# Dependence

`iconv` and `uchardet`

They are used to handle different file encoding formats, and generally exists by default in most of linux distributions.

Or you can install it through the following command:

```bash
sudo apt install libiconv-hook-dev libiconv-hook1\
libuchardet-dev libuchardet0
```
