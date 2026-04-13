>[!WARNING]
>This project is under development and stability is not guaranteed.

# Introduce

a simple lrc parsing library

# Install

## Engineering way
1. add this repository as git submodule
    ``` bash
    git submodule add <this repo> <your path>
    ```
2. Add the following statement to your CMakeLists.txt
    ``` cmake
    add_subdirectory(lrc-parser)
    target_link_libraries(your_app PRIVATE lrc_parser)
    ```

## Simple way
copy source code files to your directory, than add dependencies in your build file.

> Do you really want install this SIMPLE library to your computer?

# Usage

1. create a `LrcParser` class.
2. use `parseFile()` or `parseString()` to parse lrc file.
3. use `getData()` to get result.
4. there are two static method:
   1. `wordToLine()` to part word sync lyrics if use East Asia characters like Chinese or Japanese.
   2. `convertEncoding()` to conver enconding if you need an enconding other than UTF-8, check enable keywords on [iconv official page](https://www.gnu.org/software/libiconv/).

## Encoding Behavior

- Parser internal text is always UTF-8.
- `parseFile()` / `parseString()` will auto-detect source encoding and convert to UTF-8 before parsing.
- If you need another output encoding, use static method:

```cpp
LrcParser::LrcFile utf8_data = parser.getData();
LrcParser::LrcFile gbk_data = LrcParser::convertEncoding(utf8_data, "GBK");
```

# Dependence

`iconv` and `uchardet`

They are used to proccess different file encoding formats, and generally exists by default in most of linux distributions.

Or you can install it through the following command if use Debian:

```bash
sudo apt install libiconv-hook-dev libiconv-hook1 \
    libuchardet-dev libuchardet0
```
# Updates
- 2026-04-13: The internal encodeing uniformly uses UTF-8, and provide a static method to converting the encoding type. **Incompatible with the previous version.**