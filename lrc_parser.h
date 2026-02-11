#pragma once

#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <unordered_map>

#include <utility>

/**
 * @brief metadata of lrc file, pure data struct
 * @note `offset` and `length` are processed through strings
 */
struct LrcMetadata
{
    std::string title;      ///< lrc key: ti
    std::string artist;     ///< lrc key: ar
    std::string album;      ///< lrc key: al
    int64_t offset = 0;     ///< format: [offset:+-ms]
    std::unordered_map<std::string, std::string> attributes;    ///< unimportant metadata
};

/**
 * @brief contains a time point and the correspoding lyrics, pure data struct
 */
struct LrcUnit
{
    uint64_t time_ms;   ///< time point as ms
    std::string text;   ///< lyrics context
};

/**
 * @brief contain `LrcMetadata` and all `LrcUnit` of the file, pure data struct
 */
struct LrcFile {
    LrcMetadata metadata;           ///< metadata
    std::vector<LrcUnit> lyrics;    ///< lyrics
};


/**
 * @brief parse the lrc file and output it in the LrcFile format
 */
class LrcParser
{
public:
    /**
     * @brief constructor, default behavior
     */
    LrcParser();

    /**
     * @brief destructor, default behavior
     */
    ~LrcParser();

    /**
     * @brief clear result
     */
    void clear();

    /**
     * @brief parse file through the input file path
     * @param filepath [in] the path of the file to be parsed
     * @param dst_encoding [in] target encoding format, default = "UTF-8"
     * @return status of parsing file
     * @note After obtaining the file infomation, `parseString()` is called for processing
     */
    bool parseFile(const std::string& filepath, const std::string& dst_encoding = "UTF-8");

    /**
     * @brief parse input raw data directly
     * @param context [in] the raw data used for parsing
     * @param dst_encoding [in] target encoding format, default = "UTF-8"
     * @return status of parsing raw data
     */
    bool parseString(const std::string& context, const std::string& dst_encoding = "UTF-8");

    
    /**
     * @brief get parsed data
     * @return reference of parsed data (m_data)
     */
    const LrcFile& getData();
    
    /**
     * @brief move parsed data to left, then release m_data
     * @return parsed data
     * @note if there is non-stl member within the struct, `clear()` is necessary.
     * @details this method implements move semantics through std::move
     */
    LrcFile moveData();

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
    std::string normalizeLineEnding(const std::string& content);

    /**
     * @brief detect enconding format through libuchardet
     * @param raw_data [in] raw data of text
     * @return name of charset on success and "" on failure
     */
    std::string detectEncoding(const std::string& raw_data);

    /**
     * @brief matching iconv name if not same with uchardet
     * @param enconding_name [in] from uchardet
     * @warning table incomplete, no error handling
     */
    std::string mapUchardetToIconv(const std::string& enconding_name);

    /**
     * @brief convert src_charset to dst_charset
     * @param raw_data [in] original lrc data
     * @param dst_charset [in] target enconding format, must be iconv name
     * @param src_charset [in] source encoding format, must be iconv name
     * @todo manually control whether to rebain the bom if use UTF-8
     */
    std::string convertFormat(const std::string& raw_data, const std::string& dst_charset, const std::string& src_charset);


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

    LrcFile m_data;
};
