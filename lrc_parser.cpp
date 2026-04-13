#include "lrc_parser.h"

#include <regex>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <cctype>

#include <iostream>
#include <fstream>
#include <sstream>
#include <utility>

#include <iconv.h>
#include <uchardet/uchardet.h>

namespace {
bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

std::string trimUtf8Bom(const std::string& s) {
    if (s.size() >= 3 &&
        static_cast<unsigned char>(s[0]) == 0xEF &&
        static_cast<unsigned char>(s[1]) == 0xBB &&
        static_cast<unsigned char>(s[2]) == 0xBF) {
        return s.substr(3);
    }
    return s;
}
}  // namespace


LrcParser::LrcParser() {}
LrcParser::~LrcParser() {}


void LrcParser::clear() {
    m_data.lyrics.clear();
    m_data.metadata = LrcMetadata{};
    m_data.type = LrcType::Unknown;
}

const LrcParser::LrcFile& LrcParser::getData() const {
    return m_data;
}

LrcParser::LrcFile LrcParser::moveData() {
    LrcFile result = std::move(m_data);
    clear();
    return result;
}

size_t LrcParser::getUnitCount() const {
    return m_data.lyrics.size();
}

size_t LrcParser::getRowCount() const {
    if (m_data.lyrics.empty()) {
        return 0;
    }

    size_t cnt = 0;
    for (const auto& it : m_data.lyrics) {
        if (it.text.find('\n') != std::string::npos) {
            cnt++;
        }
    }
    const auto& final = m_data.lyrics.back();
    if (!final.text.empty()) {
        cnt++;
    }
    return cnt;
}

bool LrcParser::parseFile(const std::string& filepath) {
    std::fstream file;
    file.open(filepath, std::ios::in | std::ios::binary);
    if (!file) {
        std::cerr << "[WARNING] Failed to open file: " << filepath << "\n";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string raw_data = buffer.str();
    file.close();

    if (!parseString(raw_data)) {
        return false;
    }
    return true;
}

bool LrcParser::parseString(const std::string& context) {
    std::string unix_raw = normalizeLineEnding(context);
    std::string iconv_enc = mapUchardetToIconv(detectEncoding(unix_raw));
    if (iconv_enc.empty()) {
        iconv_enc = "UTF-8";
    }

    std::string utf8_raw;
    if (iequals(iconv_enc, "UTF-8") || iequals(iconv_enc, "ASCII")) {
        utf8_raw = unix_raw;
    } else {
        utf8_raw = convertFormat(unix_raw, "UTF-8", iconv_enc);
        if (utf8_raw.empty() && !unix_raw.empty()) {
            std::cerr << "[WARNING] Failed to convert source text to UTF-8: " << iconv_enc << "\n";
            return false;
        }
    }

    utf8_raw = trimUtf8Bom(utf8_raw);
    if (!parse(utf8_raw)) {
        std::cout << "[WARNING] Failed to parse lrc context";
        return false;
    }
    return true;
}


std::string LrcParser::normalizeLineEnding(const std::string& content) {
    std::string data = content;
    std::regex crlf("\r\n");
    data = std::regex_replace(data, crlf, "\n");
    std::regex cr("\r");
    data = std::regex_replace(data, cr, "\n");
    return data;
}

std::string LrcParser::detectEncoding(const std::string& raw_data) {
    uchardet_t ud = uchardet_new();
    int ret_code;
    ret_code = uchardet_handle_data(ud, raw_data.c_str(), raw_data.size());
    if (0 != ret_code) {
        std::cerr << "[INFO] Failed to detevt file's encoding format, error code=" << ret_code << "\n";
        uchardet_delete(ud);
        return std::string();
    }
    uchardet_data_end(ud);
    std::string encoding = uchardet_get_charset(ud);
    uchardet_delete(ud);
    return encoding;
}

std::string LrcParser::mapUchardetToIconv(const std::string& enconding_name) {
    const auto& enc = enconding_name;
    // some common coding name;
    if (enc == "Windows-1250") return "CP1250";
    if (enc == "Windows-1251") return "CP1251";
    if (enc == "Windows-1252") return "CP1252";
    if (enc == "Windows-1253") return "CP1253";
    if (enc == "Windows-1255") return "CP1255";
    if (enc == "Windows-1257") return "CP1257";
    // uchardet only: Windows-1258
    // iconv only: CP1254 CP1256
    return enc;
}

std::string LrcParser::convertFormat(const std::string& raw_data, const std::string& dst_charset, const std::string& src_charset) {
    if (dst_charset == src_charset) {
        return raw_data;
    }

    iconv_t conv = iconv_open(dst_charset.c_str(), src_charset.c_str());
    if (conv == (iconv_t)(-1)) {
        std::cerr << "[ERROR] iconv_open failed: " << strerror(errno) << "(" << src_charset << "->" << dst_charset << ")\n";
        return "";
    }
    std::string ret_str;
    size_t in_len = raw_data.size();
    if (in_len == 0) {
        iconv_close(conv);
        return "";
    }

    const size_t buf_size = 4096;
    std::vector<char> output_buffer(buf_size);

    char* in_buf = const_cast<char*>(raw_data.c_str());
    size_t in_bytes_left = in_len;
    
    while (in_bytes_left > 0) {
        char* out_buf = output_buffer.data();
        size_t out_bytes_left = output_buffer.size();

        size_t result = iconv(conv, &in_buf, &in_bytes_left, &out_buf, &out_bytes_left);

        size_t bytes_written = output_buffer.size() - out_bytes_left;
        if (bytes_written > 0) {
            ret_str.append(output_buffer.data(), bytes_written);
        }

        if ((size_t)(-1) == result) {
            if (errno == E2BIG) {
                // buffer is full
                continue;
            } else if (errno == EILSEQ) {
                // meet invalid str
                // deal: ignore this and replace with `?`
                in_buf++;
                in_bytes_left--;
                ret_str.push_back('?');
                iconv(conv, NULL, NULL, NULL, NULL);
            } else if (errno == EINVAL) {
                std::cerr << "[WARNING] Incomplete sequence at end of string\n";
                break;
            } else {
                std::cerr << "[ERROR] iconv failed: " << strerror(errno) << "\n";
                break;
            }
        }
    }
    iconv_close(conv);

    return ret_str;
}


bool LrcParser::parse(const std::string& raw_data) {
    ///< 1. token split
    // format of token = `[tag] text`
    // if multiple tags reuse a piece of text, tags will be splited
    clear();
    std::vector<LrcUnit> separated_units;
    std::vector<int64_t> pending_timestamps;    // 暂存复用的时间戳

    size_t cursor = 0;

    size_t first_bracket = raw_data.find('[', cursor);
    if (first_bracket == std::string::npos) {
        // if meet invalid format
        return false;
    }
    cursor = first_bracket;
    while (cursor < raw_data.size()) {
        // 1. extract tags
        size_t closing_bracket = raw_data.find(']', cursor);
        if (std::string::npos == closing_bracket) {
            // error: no matching right bracket
            closing_bracket = raw_data.size();
        } else {
            std::string tag_content = raw_data.substr(cursor+1, closing_bracket-cursor-1);
            int64_t ts = parseTimestamp(tag_content);
            if (-1 != ts) {
                // is timestamp
                pending_timestamps.push_back(ts);
            } else {    // is metadata
                if (!parseTag(tag_content)) {
                    std::cerr << "[ERROR] Failed to parse tag: " << tag_content << "\n";
                }
            }

            cursor = closing_bracket + 1;   // after '['
        }

        ///< 2. extract text (until next '[' or EOF)
        size_t next_bracket = raw_data.find('[', cursor);
        if (std::string::npos == next_bracket) {
            next_bracket = raw_data.size();
        }

        if (next_bracket > cursor) {
            std::string text = raw_data.substr(cursor, next_bracket - cursor);
            // Keep single line break but need
            // @todo trim
            if (!pending_timestamps.empty()) {
                for (int64_t ts : pending_timestamps) {
                    separated_units.push_back({(uint64_t)ts, text});
                }
                pending_timestamps.clear();
            }
        }
        cursor = next_bracket;
    }
    std::sort(separated_units.begin(), separated_units.end(),[](const LrcUnit& a, const LrcUnit& b){
        return a.time_ms < b.time_ms;
    });
    m_data.lyrics = std::move(separated_units);

    ///< detect lrc type, see `LrcParser::LrcType`
    m_data.type = detectLrcType(m_data.lyrics);

    return !m_data.lyrics.empty();
}

int64_t LrcParser::parseTimestamp(const std::string& timestamp) {
    if (timestamp.empty()) {
        return -1;
    }
    // valid format: `mm:ss`, `mm:ss.xx`, `mm:ss.xxx`, especially: `mm:ss.x`
    static const std::regex pattern(R"(^(\d{1,3}):(\d{2})(?:\.(\d{1,3}))?$)");
    std::smatch matches;
    if (!std::regex_match(timestamp, matches, pattern)) {
        return -1;
    }

    try {
        int mins = std::stoi(matches[1]);
        int secs = std::stoi(matches[2]);

        if (mins < 0 || secs < 0 || secs >= 60) {
            std::cout << "[WARNING] Invalid format: timestamp(s) out of range\n";
            return -1;
        }

        int ms = 0;
        if (matches[3].matched) {
            std::string ms_str = matches[3];
            if (ms_str.length() == 3) {
                ms = std::stoi(ms_str);
            } else if (ms_str.length() == 2) {
                ms = std::stoi(ms_str) * 10;
            } else if (ms_str.length() == 1) {
                ms = std::stoi(ms_str) * 100;
            } else {
                std::cout << "[WARNING] Invalid format: unsupported millisecond format";
                return -1;
            }
            
            if (ms < 0 || ms >= 1000) {
                return -1;
            }
        }
        return (mins*60 + secs) * 1000 + ms;
    }
    catch(const std::exception& e)
    {
        return -1;
    }
}

bool LrcParser::parseTag(const std::string& tag) {
    // tag format: `type:data`
    size_t colon = tag.find(':');
    if (colon == std::string::npos) {
        ///< @note if tag like `[Chorus]`, it should be regarded comment
        return true;
    }
    std::string type = tag.substr(0, colon);
    std::string data = tag.substr(colon + 1);

    if (type == "ti") {
        m_data.metadata.title = data;
    } else if (type == "ar") {
        m_data.metadata.artist = data;
    } else if (type == "al") {
        m_data.metadata.album = data;
    } else if (type == "offset") {
        std::string offset_time_str;
        bool isPositive;
        if (data.empty()) {
            m_data.metadata.offset = 0;
            return true;
        }

        if (data.at(0) == '-') {
            isPositive = false;
            offset_time_str = data.substr(1);
        } else if (data.at(0) == '+') {
            isPositive = true;
            offset_time_str = data.substr(1);
        } else {
            isPositive = true;
            offset_time_str = data;
        }
        int64_t time_ms = 0;
        try {
            time_ms = std::stoll(offset_time_str);
        } catch (...) { time_ms = 0; }
        m_data.metadata.offset = isPositive ? time_ms : -time_ms ;
    }
    else {
        m_data.metadata.attributes[type] = data;
    }
    return true;
}


LrcParser::LrcType LrcParser::detectLrcType(const std::vector<LrcUnit>& lyrics) {
    using LrcType = LrcParser::LrcType;
    using LrcUnit = LrcParser::LrcUnit;

    if (lyrics.empty())
        return LrcType::Unknown;

    double total_duration = (lyrics.back().time_ms - lyrics.front().time_ms) / 1000.0;
    if (total_duration <= 0.1)  // too short
        return LrcType::Unknown;
    
    double density = lyrics.size() / total_duration;

    if (density > 1.2)
        return LrcType::WordSync;
    if (density < 0.6)
        return LrcType::LineSync;
    
    // number of char / number of labels
    size_t total_chars = 0;
    for (const LrcUnit& unit : lyrics) {
        total_chars += count_visible_chars(unit.text);
    }

    double char_per_tag = static_cast<double>(total_chars) / lyrics.size();
    if (char_per_tag < 4.0) {
        return LrcType::WordSync;
    }
    return LrcType::LineSync;
}

LrcParser::LrcFile LrcParser::wordToLine(const LrcFile& lrc) {
    using LrcType = LrcParser::LrcType;

    if (lrc.lyrics.empty() || lrc.type != LrcType::WordSync) {
        return lrc;
    }

    LrcFile file;
    file.metadata = lrc.metadata;
    file.type = LrcType::LineSync;

    uint64_t line_start_time = 0;
    std::string line_text;
    bool line_opened = false;

    for (const auto& unit : lrc.lyrics) {
        if (!line_opened) {
            line_start_time = unit.time_ms;
            line_opened = true;
        }

        line_text += unit.text;
        if (unit.text.find('\n') != std::string::npos) {
            file.lyrics.push_back({line_start_time, line_text});
            line_text.clear();
            line_opened = false;
        }
    }

    if (line_opened && !line_text.empty()) {
        file.lyrics.push_back({line_start_time, line_text});
    }

    return file;
}

LrcParser::LrcFile LrcParser::convertEncoding(const LrcFile& lrc, const std::string& dst_encoding) {
    if (dst_encoding.empty() || iequals(dst_encoding, "UTF-8")) {
        return lrc;
    }

    LrcFile converted = lrc;

    auto convert_field = [&dst_encoding](std::string& field) {
        if (field.empty()) {
            return;
        }
        std::string out = LrcParser::convertFormat(field, dst_encoding, "UTF-8");
        if (!out.empty()) {
            field = std::move(out);
        }
    };

    convert_field(converted.metadata.title);
    convert_field(converted.metadata.artist);
    convert_field(converted.metadata.album);

    std::unordered_map<std::string, std::string> converted_attrs;
    converted_attrs.reserve(converted.metadata.attributes.size());
    for (const auto& kv : converted.metadata.attributes) {
        std::string key = kv.first;
        std::string value = kv.second;
        convert_field(key);
        convert_field(value);
        converted_attrs.emplace(std::move(key), std::move(value));
    }
    converted.metadata.attributes = std::move(converted_attrs);

    for (auto& unit : converted.lyrics) {
        convert_field(unit.text);
    }

    return converted;
}


size_t LrcParser::count_visible_chars(const std::string& str) {
    size_t count = 0;
    size_t i = 0;
    while (i < str.size()) {
        unsigned char c = static_cast<unsigned char>(str[i]);

        // skip format char
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') {
            i += 1;
            continue;
        }

        // judge UTF-8 char length
        size_t char_len = 1;
        if ((c & 0x80) == 0)         char_len = 1;  // 0xxx_xxxx
        else if ((c & 0xE0) == 0xC0) char_len = 2;  // 110x_xxxx
        else if ((c & 0xF0) == 0xE0) char_len = 3;  // 1110_xxxx
        else if ((c & 0xF8) == 0xF0) char_len = 4;  // 1111_0xxx
        else char_len = 1;

        count++;
        i += char_len;
    }
    return count;
}
