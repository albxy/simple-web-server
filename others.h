#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/locale.hpp>
#include <boost/filesystem.hpp>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
// 配置结构体
struct ServerConfig {
    std::string address;
    unsigned short port;
    std::string doc_root;
    int threads;
    std::string cert_file;
    std::string key_file;
    std::string custom_file;
    std::string log_file;
	std::string upload_path;
};

// 从配置文件加载配置
ServerConfig load_config(const std::string& config_file) {
    try
    {
        boost::property_tree::ptree pt;
        boost::property_tree::ini_parser::read_ini(config_file, pt);

        ServerConfig config;
        config.address = pt.get<std::string>("server.address", "0.0.0.0");
        config.port = pt.get<unsigned short>("server.port", 443);
        config.doc_root = pt.get<std::string>("server.doc_root", ".");
        config.threads = pt.get<int>("server.threads", 1);
        config.cert_file = pt.get<std::string>("ssl.cert_file", "C:\\Users\\15461\\Desktop\\server\\x64\\Release\\server.crt");
        config.key_file = pt.get<std::string>("ssl.key_file", "C:\\Users\\15461\\Desktop\\server\\x64\\Release\\server.key");
        config.custom_file = pt.get<std::string>("server.custom_file", "C:\\Users\\15461\\Desktop\\server\\x64\\Release\\index.html");
        config.log_file = pt.get<std::string>("server.log_file", "server.log");
        config.upload_path = pt.get<std::string>("server.upload_path", "C:\\Users\\15461\\Desktop\\server\\x64\\Release\\temp\\");
		return config;
	}
	catch (const boost::property_tree::ini_parser_error& e)
	{
		std::cerr << "Error loading config file: " << e.what() << std::endl;
		throw;
    }
    
}
ServerConfig config = load_config("C:\\Users\\15461\\Desktop\\server\\x64\\Release\\config.ini");
bool is_valid_utf8(const std::string& str) {
    try {
        // 使用Boost.Locale的验证方式
        boost::locale::conv::utf_to_utf<char>(str);
        return true;
    }
    catch (const boost::locale::conv::conversion_error&) {
        return false;
    }
}

std::string UTF8ToLocal(const std::string& utf8_str) {
    try {
        if (!is_valid_utf8(utf8_str)) {
            throw std::runtime_error("Invalid UTF-8 input");
        }

        boost::locale::generator gen;
        std::locale loc = gen.generate("");
        return boost::locale::conv::from_utf(utf8_str, loc);
    }
    catch (const std::exception& e) {
        std::cerr << "Conversion error: " << e.what() << std::endl;
        return utf8_str;
    }
}
inline
void
load_server_certificate(boost::asio::ssl::context& ctx, const ServerConfig& config)
{
    ctx.set_options(ssl::context::default_workarounds
        | ssl::context::no_sslv2
        | ssl::context::no_sslv3
        | ssl::context::no_tlsv1
        | ssl::context::no_tlsv1_1);

    ctx.use_certificate_file(config.cert_file, ssl::context::pem);
    ctx.use_private_key_file(config.key_file, ssl::context::pem);
}

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
    if (ec == net::ssl::error::stream_truncated)
        return;

    std::cerr << what << ": " << ec.message() << "\n";
}