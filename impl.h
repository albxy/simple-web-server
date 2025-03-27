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
#include "others.h"
#include "uploads.h"
#include "users.h"
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace fs = boost::filesystem;  // 用于文件操作
namespace ptree = boost::property_tree;
std::string decoded_req_target;

template <class Body, class Allocator>
http::message_generator
handle_get_request(http::request<Body, http::basic_fields<Allocator>>& req, const std::string &p4,std::string &path)
{
    beast::error_code ec;
    // Returns a not found response
    auto const not_found = //404文件响应
        [&req,&p4](beast::string_view target)
        {
            http::response<http::file_body> res{ http::status::ok, req.version() };
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            beast::error_code tec;
            res.body().open(p4.c_str(), beast::file_mode::read, tec);
            res.prepare_payload();
            return res;
        };
    // Returns a server error response
    auto const server_error =
        [&req](beast::string_view what)
        {
            http::response<http::string_body> res{ http::status::internal_server_error, req.version() };
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body() = "An error occurred: '" + std::string(what) + "'";
            res.prepare_payload();
            return res;
        };
    http::file_body::value_type body;
    body.open(path.c_str(), beast::file_mode::scan, ec);

    // Handle the case where the file doesn't exist
    if (ec == beast::errc::no_such_file_or_directory)
        return not_found(req.target());

    // Cache the size since we need it after the move
    auto const size = body.size();
        fs::path directoryPath(path);
        if (fs::is_directory(directoryPath))
        {
            std::string temp;
            http::response<http::string_body> res{ http::status::ok,req.version() };
            for (fs::directory_iterator it(directoryPath); it != fs::directory_iterator(); ++it)
            {
                const auto& path = it->path();
                const auto& status = it->status();

                // 获取文件（夹）的大小
                std::uintmax_t size = fs::is_directory(status) ? 0 : fs::file_size(path);
                if (fs::is_directory(it->status()))
                {
                    temp = it->path().filename().string() + "*Folder*" + std::to_string(size) + '\n';
                    temp = AnsiToUtf8(temp);
                    res.body().append(temp);
                }
                else
                {
                    temp = it->path().filename().string() + "*File*" + std::to_string(size) + '\n';
                    temp = AnsiToUtf8(temp);
                    res.body().append(temp);
                }
            }
            if (res.body().empty())
                res.body() = AnsiToUtf8("Empty");
            //cout << "resbody:"<<res.body() << endl;
            res.prepare_payload();
            return res;

        }
        // Check if the client has a cached version
        std::string eTag = calculateETag(path);
        auto it = req.find(http::field::if_none_match);
        if (it != req.end())
        {
            std::string clientETag = it->value();
            if (clientETag == eTag)
            {
                http::response<http::empty_body> res{ http::status::not_modified, req.version() };
                res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                res.keep_alive(req.keep_alive());
                return res;
            }
        }

        http::response<http::file_body> res{
            std::piecewise_construct,
            std::make_tuple(std::move(body)),
            std::make_tuple(http::status::ok, req.version()) };
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, mime_type(path));
        res.set(http::field::etag, eTag);//设置eTag
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        return res;

    // Handle an unknown error
    if (ec)
    {
        cout << "EC:" << ec.message() << endl;
        return server_error(ec.message());
    }
}
template <class Body, class Allocator>
http::message_generator
handle_post_request(http::request<Body, http::basic_fields<Allocator>>&& req,
    const string &cookie_username,const string &cookie_password)
{
	//Returns a plain response
	auto const plain_request =
		[&req](beast::string_view why)
		{
			http::response<http::string_body> res{ http::status::ok, req.version() };
			res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
			res.set(http::field::content_type, "text/plain");
			res.keep_alive(req.keep_alive());
			res.body() = std::string(why);
			res.prepare_payload();
			return res;
		};
    std::string req_username;
	std::string req_password;
	std::string request_body = req.body();
    if (decoded_req_target == "/login")
    {
        size_t pos = request_body.find("username=");
        if (pos != std::string::npos) {
            pos += 9;
            size_t end_pos = request_body.find("&", pos);
            req_username = request_body.substr(pos, end_pos - pos);
        }
        pos = request_body.find("password=");
        if (pos != std::string::npos) {
            pos += 9;
            req_password = request_body.substr(pos);
        }
        if (req_username.size() > 32 || req_password.size() > 32 || req_password.size() == 0 || req_username.size() == 0)
            return plain_request("Invalid");
        if (UAP(req_username, req_password))
            return plain_request("OK");
        else
            return plain_request("Username or Password wrong");
    }
    else if (decoded_req_target == "/register")
    {
        size_t pos = request_body.find("username=");
        if (pos != std::string::npos) {
            pos += 9;
            size_t end_pos = request_body.find("&", pos);
            if (end_pos == std::string::npos) {
                return plain_request("Invalid");
            }
            else {
                req_username = request_body.substr(pos, end_pos - pos);
            }
        }
        pos = request_body.find("password=");
        if (pos != std::string::npos) {
            pos += 9;
            req_password = request_body.substr(pos);
        }
        if (req_username.size() > 32 || req_password.size() > 32 || req_password.size() == 0 || req_username.size() == 0)
            return plain_request("Invalid");
        if (!AddUser(req_username, req_password)) {
            return plain_request("Repeated");
        }
        else
            return plain_request("OK");
    }
    else if (decoded_req_target == "/change")
    {

        if (cookie_username == "guest")
            return plain_request("No access");

        size_t pos = request_body.find("username=");
        if (pos != std::string::npos) {
            pos += 9;
            size_t end_pos = request_body.find("&", pos);
            if (end_pos == std::string::npos) {
                return plain_request("Invalid");
            }
            else {
                req_username = request_body.substr(pos, end_pos - pos);
            }
        }
        pos = request_body.find("password=");
        if (pos != std::string::npos) {
            pos += 9;
            req_password = request_body.substr(pos);
        }
        if (req_username.size() > 32 || req_password.size() > 32 || req_password.size() == 0 || req_username.size() == 0)
            return plain_request("Invalid");
        if (is_Repeated(req_username) && req_username != cookie_username)
            return plain_request("Repeated");
        //std::cout << "LLLL:" << cookie_username << " " << req_username << " " << req_password << '\n';
        if (CUP(cookie_username, req_username, req_password))
            return plain_request("OK");
        else
            return plain_request("CUP failed");
    }
    else if (decoded_req_target == "/del")
    {
        if (cookie_username == "guest")
            return plain_request("No access");
        if (DelUser(cookie_username))
            return plain_request("OK");
        else
            return plain_request("del failed");
    }
    else
		return plain_request("unknown post target");
}