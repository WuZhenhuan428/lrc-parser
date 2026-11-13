#include "lrc_parser.h"

#include <iconv.h>
#include <uchardet/uchardet.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <ranges>
#include <regex>
#include <sstream>
#include <utility>

namespace
{
bool iequals(const std::string& a, const std::string& b)
{
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

std::string trim_utf8_bom(const std::string& s)
{
    if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF &&
        static_cast<unsigned char>(s[1]) == 0xBB &&
        static_cast<unsigned char>(s[2]) == 0xBF) {
        return s.substr(3);
    }
    return s;
}
} // namespace

void LrcParser::clear()
{
    data_.lyrics.clear();
    data_.metadata = LrcMetadata{};
    data_.type     = LrcType::Unknown;
}

const LrcParser::LrcFile& LrcParser::get_data() const
{
    return data_;
}

std::string LrcParser::format() const
{
    if (this->data_.type == LrcType::Unknown) {
        return {};
    }
    std::stringstream ss;
    // attributes
    const auto& meta   = data_.metadata;
    bool has_attribute = false;
    if (!meta.title.empty()) {
        ss << std::format("[ti:{}]\n", meta.title);
        has_attribute = true;
    }
    if (!meta.artist.empty()) {
        ss << std::format("[ar:{}]\n", meta.artist);
        has_attribute = true;
    }
    if (!meta.album.empty()) {
        ss << std::format("[al:{}]\n", meta.album);
        has_attribute = true;
    }
    if (meta.offset != 0) {
        ss << std::format("[offset:{}]\n", meta.offset);
        has_attribute = true;
    }
    if (!meta.attributes.empty()) {
        for (const auto& [k, v] : meta.attributes) {
            ss << std::format("[{}:{}]\n", k, v);
        }
        has_attribute = true;
    }
    if (has_attribute) {
        ss << "\n";
    };
    // texts
    const auto& lrc = data_.lyrics;
    for (const auto& [ms, text] : lrc) {
        uint64_t ms_tail  = ms % 1000;
        uint64_t sec      = ms / 1000;
        uint64_t sec_tail = sec % 60;
        uint64_t min      = sec / 60;
        uint64_t min_tail = min % 60;
        uint64_t hour     = min / 60;
        // "[HH:MM:SS.mmm] text"
        // text contains '\n'
        ss << std::format("[{}{:02}:{:02}.{:03}] {}",
                          hour == 0 ? "" : std::format("{:02}:", hour),
                          min_tail, sec_tail, ms_tail, text);
    }
    return ss.str();
}

LrcParser::LrcFile LrcParser::move_data()
{
    LrcFile result = std::move(data_);
    clear();
    return result;
}

size_t LrcParser::get_row_count(const LrcFile& lrc)
{
    if (lrc.lyrics.empty()) {
        return 0;
    }

    size_t cnt = 0;
    for (const auto& it : lrc.lyrics) {
        if (it.text.contains('\n')) {
            cnt++;
        }
    }
    const auto& final = lrc.lyrics.back();
    if (!final.text.empty()) {
        cnt++;
    }
    return cnt;
}

bool LrcParser::parse_file(const std::string& filepath)
{
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

    if (!parse_string(raw_data)) {
        return false;
    }
    return true;
}

bool LrcParser::parse_string(const std::string& context)
{
    std::string unix_raw  = normalize_line_ending(context);
    std::string iconv_enc = map_uchardet_to_iconv(detect_encoding(unix_raw));
    if (iconv_enc.empty()) {
        iconv_enc = "UTF-8";
    }

    std::string utf8_raw;
    if (iequals(iconv_enc, "UTF-8") || iequals(iconv_enc, "ASCII")) {
        utf8_raw = unix_raw;
    } else {
        utf8_raw = convert_format(unix_raw, "UTF-8", iconv_enc);
        if (utf8_raw.empty() && !unix_raw.empty()) {
            std::cerr << "[WARNING] Failed to convert source text to UTF-8: "
                      << iconv_enc << "\n";
            return false;
        }
    }

    utf8_raw = trim_utf8_bom(utf8_raw);
    if (!parse(utf8_raw)) {
        std::cout << "[WARNING] Failed to parse lrc context";
        return false;
    }
    return true;
}

std::string LrcParser::normalize_line_ending(const std::string& content)
{
    std::string data = content;
    std::regex crlf("\r\n");
    data = std::regex_replace(data, crlf, "\n");
    std::regex cr("\r");
    data = std::regex_replace(data, cr, "\n");
    return data;
}

std::string LrcParser::detect_encoding(const std::string& raw_data)
{
    uchardet_t ud = uchardet_new();
    int ret_code  = uchardet_handle_data(ud, raw_data.c_str(), raw_data.size());
    if (0 != ret_code) {
        std::cerr
            << "[INFO] Failed to detect file's encoding format, error code="
            << ret_code << "\n";
        uchardet_delete(ud);
        return {};
    }
    uchardet_data_end(ud);
    std::string encoding = uchardet_get_charset(ud);
    uchardet_delete(ud);
    return encoding;
}

std::string LrcParser::map_uchardet_to_iconv(const std::string& enconding_name)
{
    const auto& enc = enconding_name;
    // some common coding name;
    if (enc == "Windows-1250") {
        return "CP1250";
    }
    if (enc == "Windows-1251") {
        return "CP1251";
    }
    if (enc == "Windows-1252") {
        return "CP1252";
    }
    if (enc == "Windows-1253") {
        return "CP1253";
    }
    if (enc == "Windows-1255") {
        return "CP1255";
    }
    if (enc == "Windows-1257") {
        return "CP1257";
    }
    // uchardet only: Windows-1258
    // iconv only: CP1254 CP1256
    return enc;
}

std::string LrcParser::convert_format(std::string raw_data,
                                      const std::string& dst_charset,
                                      const std::string& src_charset)
{
    if (dst_charset == src_charset) {
        return raw_data;
    }

    iconv_t conv = iconv_open(dst_charset.c_str(), src_charset.c_str());
    if (conv == (iconv_t)(-1)) {
        std::cerr << "[ERROR] iconv_open failed: " << strerror(errno) << "("
                  << src_charset << "->" << dst_charset << ")\n";
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

    char* in_buf         = const_cast<char*>(raw_data.c_str());
    size_t in_bytes_left = in_len;

    while (in_bytes_left > 0) {
        char* out_buf         = output_buffer.data();
        size_t out_bytes_left = output_buffer.size();

        size_t result =
            iconv(conv, &in_buf, &in_bytes_left, &out_buf, &out_bytes_left);

        size_t bytes_written = output_buffer.size() - out_bytes_left;
        if (bytes_written > 0) {
            ret_str.append(output_buffer.data(), bytes_written);
        }

        if (static_cast<size_t>(-1) == result) {
            if (errno == E2BIG) {
                // buffer is full
                continue;
            }
            if (errno == EILSEQ) {
                // meet invalid str
                // deal: ignore this and replace with `?`
                in_buf++;
                in_bytes_left--;
                ret_str.push_back('?');
                iconv(conv, nullptr, nullptr, nullptr, nullptr);
            } else if (errno == EINVAL) {
                std::cerr << "[WARNING] Incomplete sequence at end of string\n";
                break;
            } else {
                std::cerr << "[ERROR] iconv failed: " << strerror(errno)
                          << "\n";
                break;
            }
        }
    }
    iconv_close(conv);

    return ret_str;
}

bool LrcParser::parse(const std::string& raw_data)
{
    ///< 1. token split
    // format of token = `[tag] text`
    // if multiple tags reuse a piece of text, tags will be splited
    clear();
    std::vector<LrcUnit> separated_units;
    std::vector<int64_t> pending_timestamps; // 暂存复用的时间戳

    size_t cursor        = 0;

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
            std::string tag_content =
                raw_data.substr(cursor + 1, closing_bracket - cursor - 1);
            int64_t ts = parse_timestamp(tag_content);
            if (-1 != ts) {
                // is timestamp
                pending_timestamps.push_back(ts);
            } else { // is metadata
                if (!parse_tag(tag_content)) {
                    std::cerr << "[ERROR] Failed to parse tag: " << tag_content
                              << "\n";
                }
            }

            cursor = closing_bracket + 1; // after '['
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
                    separated_units.push_back(
                        {static_cast<uint64_t>(ts), text});
                }
                pending_timestamps.clear();
            }
        }
        cursor = next_bracket;
    }
    std::ranges::sort(separated_units, {}, &LrcUnit::time_ms);
    data_.lyrics = std::move(separated_units);

    ///< detect lrc type, see `LrcParser::LrcType`
    data_.type   = detect_lrc_type(data_.lyrics);

    return !data_.lyrics.empty();
}

int64_t LrcParser::parse_timestamp(const std::string& timestamp)
{
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
            std::cout
                << "[WARNING] Invalid format: timestamp(s) out of range\n";
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
                std::cout << "[WARNING] Invalid format: unsupported "
                             "millisecond format";
                return -1;
            }

            if (ms < 0 || ms >= 1000) {
                return -1;
            }
        }
        return (((mins * 60) + secs) * 1000) + ms;
    } catch (const std::exception& e) {
        return -1;
    }
}

bool LrcParser::parse_tag(const std::string& tag)
{
    // tag format: `type:data`
    size_t colon = tag.find(':');
    if (colon == std::string::npos) {
        ///< @note if tag like `[Chorus]`, it should be regarded comment
        return true;
    }
    std::string type = tag.substr(0, colon);
    std::string data = tag.substr(colon + 1);

    if (type == "ti") {
        data_.metadata.title = data;
    } else if (type == "ar") {
        data_.metadata.artist = data;
    } else if (type == "al") {
        data_.metadata.album = data;
    } else if (type == "offset") {
        std::string offset_time_str;
        bool is_positive = true;
        if (data.empty()) {
            data_.metadata.offset = 0;
            return true;
        }

        if (data.at(0) == '-') {
            is_positive     = false;
            offset_time_str = data.substr(1);
        } else if (data.at(0) == '+') {
            is_positive     = true;
            offset_time_str = data.substr(1);
        } else {
            is_positive     = true;
            offset_time_str = data;
        }
        int64_t time_ms = 0;
        try {
            time_ms = std::stoll(offset_time_str);
        } catch (...) {
            time_ms = 0;
        }
        data_.metadata.offset = is_positive ? time_ms : -time_ms;
    } else {
        data_.metadata.attributes[type] = data;
    }
    return true;
}

LrcParser::LrcType
LrcParser::detect_lrc_type(const std::vector<LrcUnit>& lyrics)
{
    using LrcType = LrcParser::LrcType;
    using LrcUnit = LrcParser::LrcUnit;

    if (lyrics.empty()) {
        return LrcType::Unknown;
    }

    double total_duration =
        (static_cast<double>(lyrics.back().time_ms - lyrics.front().time_ms)) /
        1000.0;
    if (total_duration <= 0.1) { // too short
        return LrcType::Unknown;
    }

    double density = static_cast<double>(lyrics.size()) / total_duration;

    if (density > 1.2) {
        return LrcType::WordSync;
    }
    if (density < 0.6) {
        return LrcType::LineSync;
    }

    // number of char / number of labels
    size_t total_chars = 0;
    for (const LrcUnit& unit : lyrics) {
        total_chars += count_visible_chars(unit.text);
    }

    double char_per_tag =
        static_cast<double>(total_chars) / static_cast<double>(lyrics.size());
    if (char_per_tag < 4.0) {
        return LrcType::WordSync;
    }
    return LrcType::LineSync;
}

LrcParser::LrcFile LrcParser::word_to_line(const LrcFile& lrc)
{
    using LrcType = LrcParser::LrcType;

    if (lrc.lyrics.empty() || lrc.type != LrcType::WordSync) {
        return lrc;
    }

    LrcFile file;
    file.metadata            = lrc.metadata;
    file.type                = LrcType::LineSync;

    uint64_t line_start_time = 0;
    std::string line_text;
    bool line_opened = false;

    for (const auto& unit : lrc.lyrics) {
        if (!line_opened) {
            line_start_time = unit.time_ms;
            line_opened     = true;
        }

        line_text += unit.text;
        if (unit.text.contains('\n')) {
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

LrcParser::LrcFile LrcParser::convert_encoding(const LrcFile& lrc,
                                               const std::string& dst_encoding)
{
    if (dst_encoding.empty() || iequals(dst_encoding, "UTF-8")) {
        return lrc;
    }

    LrcFile converted  = lrc;

    auto convert_field = [&dst_encoding](std::string& field) {
        if (field.empty()) {
            return;
        }
        std::string out =
            LrcParser::convert_format(field, dst_encoding, "UTF-8");
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
        std::string key   = kv.first;
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

size_t LrcParser::count_visible_chars(const std::string& str)
{
    size_t count = 0;
    size_t i     = 0;
    while (i < str.size()) {
        unsigned char c = static_cast<unsigned char>(str[i]);

        // skip format char
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') {
            i += 1;
            continue;
        }

        // judge UTF-8 char length
        size_t char_len = 1;
        if ((c & 0xE0) == 0xC0) {
            char_len = 2;
        } // 110x_xxxx
        else if ((c & 0xF0) == 0xE0) {
            char_len = 3;
        } // 1110_xxxx
        else if ((c & 0xF8) == 0xF0) {
            char_len = 4;
        } // 1111_0xxx
        else {
            char_len = 1;
        }

        count++;
        i += char_len;
    }
    return count;
}
