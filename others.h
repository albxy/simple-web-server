#pragma once
#include <openssl/evp.h>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <iomanip>
#include <map>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace fs = boost::filesystem;  // 用于文件操作
namespace ptree = boost::property_tree;

// Return a reasonable mime type based on the extension of a file.
beast::string_view
mime_type(beast::string_view path)
{
    using beast::iequals;
    auto const ext = [&path]
        {
            auto const pos = path.rfind(".");
            if (pos == beast::string_view::npos)
                return beast::string_view{};
            return path.substr(pos);
        }();
    if (iequals(ext, ".htm"))  return "text/html";
    if (iequals(ext, ".html")) return "text/html";
    if (iequals(ext, ".php"))  return "text/html";
    if (iequals(ext, ".css"))  return "text/css";
    if (iequals(ext, ".txt"))  return "text/plain";
    if (iequals(ext, ".js"))   return "application/javascript";
    if (iequals(ext, ".json")) return "application/json";
    if (iequals(ext, ".xml"))  return "application/xml";
    if (iequals(ext, ".swf"))  return "application/x-shockwave-flash";
    if (iequals(ext, ".flv"))  return "video/x-flv";
    if (iequals(ext, ".png"))  return "image/png";
    if (iequals(ext, ".jpe"))  return "image/jpeg";
    if (iequals(ext, ".jpeg")) return "image/jpeg";
    if (iequals(ext, ".jpg"))  return "image/jpeg";
    if (iequals(ext, ".gif"))  return "image/gif";
    if (iequals(ext, ".bmp"))  return "image/bmp";
    if (iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
    if (iequals(ext, ".tiff")) return "image/tiff";
    if (iequals(ext, ".tif"))  return "image/tiff";
    if (iequals(ext, ".svg"))  return "image/svg+xml";
    if (iequals(ext, ".svgz")) return "image/svg+xml";
    return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string
path_cat(
    beast::string_view base,
    beast::string_view path)
{
    if (base.empty())
        return std::string(path);
    std::string result(base);
#ifdef BOOST_MSVC
    char constexpr path_separator = '\\';
    if (result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
    for (auto& c : result)
        if (c == '/')
            c = path_separator;
#else
    char constexpr path_separator = '/';
    if (result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
#endif
    return result;
}
std::string UTF8ToLocal(const std::string& utf8Str) {
    // 获取UTF-8字符串的长度
    int utf8Length = static_cast<int>(utf8Str.length());

    // 将UTF-8字符串转换为宽字符（UTF-16）
    int wideLength = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), utf8Length, nullptr, 0);
    if (wideLength == 0) {
        return ""; // 转换失败
    }

    std::vector<wchar_t> wideStr(wideLength);
    if (MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), utf8Length, wideStr.data(), wideLength) == 0) {
        return ""; // 转换失败
    }

    // 将宽字符（UTF-16）转换为本地编码（ANSI）
    int localLength = WideCharToMultiByte(CP_ACP, 0, wideStr.data(), wideLength, nullptr, 0, nullptr, nullptr);
    if (localLength == 0) {
        return ""; // 转换失败
    }

    std::vector<char> localStr(localLength);
    if (WideCharToMultiByte(CP_ACP, 0, wideStr.data(), wideLength, localStr.data(), localLength, nullptr, nullptr) == 0) {
        return ""; // 转换失败
    }

    return std::string(localStr.data(), localStr.size());
}

std::string AnsiToUtf8(const std::string& ansiStr) {
    // 首先将 ANSI 字符串转换为宽字符字符串
    int wideLength = MultiByteToWideChar(CP_ACP, 0, ansiStr.c_str(), static_cast<int>(ansiStr.length()), nullptr, 0);
    if (wideLength == 0) {
        return "";
    }
    std::wstring wideStr(wideLength, 0);
    MultiByteToWideChar(CP_ACP, 0, ansiStr.c_str(), static_cast<int>(ansiStr.length()), &wideStr[0], wideLength);

    // 然后将宽字符字符串转换为 UTF - 8 字符串
    int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), static_cast<int>(wideStr.length()), nullptr, 0, nullptr, nullptr);
    if (utf8Length == 0) {
        return "";
    }
    std::string utf8Str(utf8Length, 0);
    WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), static_cast<int>(wideStr.length()), &utf8Str[0], utf8Length, nullptr, nullptr);

    return utf8Str;
}
std::string url_decode(const std::string& url) {
    std::string decoded_str;
    for (size_t i = 0; i < url.length(); i++) {
        if (url[i] == '%') {
            // 检查是否有足够的字符用于解码
            if (i + 2 >= url.length()) {
                // 非法格式，直接保留%
                decoded_str += url[i];
                continue;
            }

            // 提取两个十六进制字符
            std::string hex_str = url.substr(i + 1, 2);
            char ch = static_cast<char>(std::stoi(hex_str, nullptr, 16));

            // 将解码后的字符添加到结果中
            decoded_str += ch;

            // 跳过已处理的字符
            i += 2;
        }
        else if (url[i] == '+') {
            // '+' 解码为空格
            decoded_str += ' ';
        }
        else {
            // 直接保留其他字符
            decoded_str += url[i];
        }
    }

    return UTF8ToLocal(decoded_str);
}
// 获取文件的最后修改时间
std::string getFileModificationTime(const std::string& path) {
    struct stat fileStat;
    if (stat(path.c_str(), &fileStat) != 0) {
        return ""; // 获取失败
    }
    // 将修改时间转换为字符串
    std::ostringstream oss;
    oss << fileStat.st_mtime;
    return oss.str();
}

// 计算文件的 ETag，包含文件内容和修改时间
std::string calculateETag(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return "";
    }

    // 获取文件的最后修改时间
    std::string modificationTime = getFileModificationTime(path);
    if (modificationTime.empty()) {
        return "";
    }

    // 初始化 EVP MD5 上下文
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (mdctx == nullptr) {
        return "";
    }

    // 初始化 MD5 哈希算法
    if (EVP_DigestInit_ex(mdctx, EVP_md5(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    // 将文件修改时间加入哈希计算
    EVP_DigestUpdate(mdctx, modificationTime.c_str(), modificationTime.size());

    // 读取文件内容并更新哈希计算
    const size_t bufferSize = 8192;
    char buffer[bufferSize];
    while (file.read(buffer, bufferSize)) {
        EVP_DigestUpdate(mdctx, buffer, file.gcount());
    }
    // 处理最后一块数据
    if (file.gcount() > 0) {
        EVP_DigestUpdate(mdctx, buffer, file.gcount());
    }

    // 计算最终的 MD5 哈希值
    unsigned char md5Digest[EVP_MAX_MD_SIZE];
    unsigned int md5DigestLen;
    if (EVP_DigestFinal_ex(mdctx, md5Digest, &md5DigestLen) != 1) {
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    // 释放上下文
    EVP_MD_CTX_free(mdctx);

    // 将 MD5 哈希值转换为十六进制字符串
    std::ostringstream oss;
    for (unsigned int i = 0; i < md5DigestLen; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(md5Digest[i]);
    }

    return oss.str();
}
// Report a failure
void
fail(beast::error_code ec, char const* what)
{
    if (ec == net::ssl::error::stream_truncated)
        return;

    std::cerr << what << ": " << ec.message() << "\n";
}