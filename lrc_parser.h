#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>


/**
* @brief parse the lrc file and output it in the LrcFile format
*/
class LrcParser
{
public:
    enum class LrcType {    // todo: duet
        Unknown = 0,
        LineSync,
        WordSync
    };

    /**
     * @note `offset` and `length` are processed through strings
     */
    struct LrcMetadata
    {
        std::string title;      ///< lrc key: ti
        std::string artist;     ///< lrc key: ar
        std::string album;      ///< lrc key: al
        int64_t offset = 0;     ///< format: [offset:+-ms]
        std::unordered_map<std::string, std::string> attributes;    ///< other metadata
    };

    struct LrcUnit
    {
        uint64_t time_ms;
        std::string text;
    };

    struct LrcFile {
        LrcMetadata metadata;
        std::vector<LrcUnit> lyrics;
        LrcParser::LrcType type = LrcType::Unknown;
    };

public:
    LrcParser();
    ~LrcParser();

    /**
     * @brief clear result
     */
    void clear();

    /**
     * @brief parse file via the input file path
     * @param filepath [in] path of lrc file which to be parsed
     * @return status of parsing file
     * @note Internal storage is always UTF-8.
     */
    bool parseFile(const std::string& filepath);

    /**
     * @brief parse input raw data directly
     * @param context [in] the raw data used for parsing
     * @return status of parsing raw data
     * @note Internal storage is always UTF-8.
     */
    bool parseString(const std::string& context);

    
    /**
     * @brief get parsed data
     * @return reference of parsed data (m_data)
     */
    const LrcFile& getData() const;
    
    /**
     * @brief move parsed data to left, then release m_data
     * @return parsed data
     * @note if there is non-stl member within the struct, `clear()` is necessary.
     * @warning if use this method, getters will be lose efficacy
     */
    LrcFile moveData();

    /**
     * @return m_data.lyrics.size()
     */
    size_t getUnitCount() const;

    /**
     * @return number of lines (not lrc unit)
     * separate by line break `(\n)`. if the last line is not empty after trim, count increment automatically
     */
    size_t getRowCount() const;

    static LrcFile wordToLine(const LrcFile& lrc);

    /**
     * @brief convert UTF-8 lrc data to target encoding
     * @param lrc [in] input lrc file (UTF-8 text)
     * @param dst_encoding [in] iconv-compatible target encoding name
     * @return converted copy; if conversion of a field fails, keeps original field
     */
    static LrcFile convertEncoding(const LrcFile& lrc, const std::string& dst_encoding);

private:
    /**
     * @brief parse the input raw data through the default enconding type
     * @note method `clear()` will execute automatically
     */
    bool parse(const std::string& raw_lrc_content);

    /**
     * @brief replace CRLF/CR with LF
     * @param content [in] the text to be processed
     * @return unix newline style raw data
     */
    static std::string normalizeLineEnding(const std::string& content);

    /**
     * @brief detect enconding format through libuchardet
     * @param raw_data [in] raw data of text
     * @return name of charset on success and "" on failure
     */
    static std::string detectEncoding(const std::string& raw_data);

    /**
     * @brief matching iconv name if not same with uchardet
     * @param enconding_name [in] from uchardet
     * @warning table incomplete, no error handling
     */
    static std::string mapUchardetToIconv(const std::string& enconding_name);

    /**
     * @brief convert src_charset to dst_charset
     * @param raw_data [in] original lrc data
     * @param dst_charset [in] target enconding format, must be iconv name
     * @param src_charset [in] source encoding format, must be iconv name
     * @todo manually control whether to rebain the bom if use UTF-8
     */
    static std::string convertFormat(const std::string& raw_data, const std::string& dst_charset, const std::string& src_charset);


    /**
     * @brief parse timestamp `[mm:ss.xx]` -> ms
     * @param timestamp [in] timestamp format: standard lrc format without brackets
     * @return the number of ms of the timestamp
     * @return -1 if parse failed
     */
    int64_t parseTimestamp(const std::string& timestamp);

    /**
     * @brief pase lrc metadata tag
     * @param tag [in] input lrc metadata token without brackets
     * @return true: successed to parse
     * @return false: failed to parse
     */
    bool parseTag(const std::string& tag);

    LrcType detectLrcType(const std::vector<LrcUnit>& lyrics);

    size_t count_visible_chars(const std::string& str);


    LrcFile m_data;
};
