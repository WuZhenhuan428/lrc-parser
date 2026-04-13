#include "../lrc_parser.h"

#include <cassert>
#include <iostream>

int main() {
    LrcParser parser;

    const std::string word_sync_lrc =
        "[ti:Demo]\n"
        "[ar:Tester]\n"
        "[00:00.00]你[00:00.50]好\n"
        "[00:01.00]世[00:01.50]界\n";

    const bool ok = parser.parseString(word_sync_lrc);
    assert(ok);

    const LrcParser::LrcFile& parsed = parser.getData();
    assert(parsed.metadata.title == "Demo");
    assert(parsed.metadata.artist == "Tester");
    assert(parsed.type == LrcParser::LrcType::WordSync);
    assert(parsed.lyrics.size() == 4);

    const LrcParser::LrcFile line_sync = LrcParser::wordToLine(parsed);
    assert(line_sync.type == LrcParser::LrcType::LineSync);
    assert(line_sync.lyrics.size() == 2);
    assert(line_sync.lyrics[0].time_ms == 0);
    assert(line_sync.lyrics[0].text == "你好\n");
    assert(line_sync.lyrics[1].time_ms == 1000);
    assert(line_sync.lyrics[1].text == "世界\n");

    const LrcParser::LrcFile utf8_copy = LrcParser::convertEncoding(parsed, "UTF-8");
    assert(utf8_copy.lyrics.size() == parsed.lyrics.size());
    assert(utf8_copy.metadata.title == parsed.metadata.title);

    std::cout << "All tests passed\n";
    return 0;
}