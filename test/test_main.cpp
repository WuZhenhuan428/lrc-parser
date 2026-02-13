#include "../lrc_parser.h"

#include <iostream>
#include <fstream>
#include <string>

// usage:
// g++ test_main.cpp ../lrc_parser.cpp ../lrc_parser.h -o test.bin $(pkg-config --cflags --libs uchardet)
// $ test.bin <lrc path>

int main(int argc, char** argv) {
    std::string filepath;
    if (1 == argc) {
        std::cout << "input a file path!\n";
        return 1;
    } else if (2 == argc) {
        filepath = argv[1];
    } else if (argc >= 3) {
        std::cout << "too more arguments\n";
        return 1;
    }
    
    std::fstream file;
    file.open(filepath, std::ios::in);
    if (!file) {
        std::cerr << "[WARNING] Can not open file: " << filepath << "\n";
        return 1;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string raw_data = buffer.str();
    file.close();
    
    LrcParser parser;
    LrcFile data;
    if (!parser.parseFile(filepath, "UTF-8")) {
        std::cout << "[PARSE] Failed to parse file.\n";
    } else {
        data = parser.getData();
        std::cout << "[PARSE] al    : " << data.metadata.album << "\n";
        std::cout << "[PARSE] ar    : " << data.metadata.artist << "\n";
        std::cout << "[PARSE] offset: " << data.metadata.offset << "\n";
        std::cout << "[PARSE] ti    : " << data.metadata.title << "\n";
        for (auto lrc : data.lyrics) {
            std::cout << "<" << lrc.time_ms << ">" << lrc.text;
        }
    }
    std::cout << "\nNumber of lines: " << parser.getRowCount() << "\n";
    std::cout << "Number of units: " << parser.getUnitCount() << "\n";
}