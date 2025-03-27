#pragma once
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

// 获取请求中的 cookies 并存储在 vector<pair> 中
void GetCookies(std::string cookieStr, std::vector<std::pair<std::string, std::string>>& cookiePairs) {

    size_t start = 0;
    size_t end = 0;
    while ((end = cookieStr.find(';', start)) != std::string::npos) {
        std::string cookie = cookieStr.substr(start, end - start);
        size_t equalPos = cookie.find('=');
        if (equalPos != std::string::npos) {
            std::string name = cookie.substr(0, equalPos);
            std::string value = cookie.substr(equalPos + 1);
            // 去除首尾空格
            name.erase(0, name.find_first_not_of(" "));
            name.erase(name.find_last_not_of(" ") + 1);
            value.erase(0, value.find_first_not_of(" "));
            value.erase(value.find_last_not_of(" ") + 1);
            cookiePairs.emplace_back(std::make_pair(name, value));
        }
        start = end + 1;
    }
    // 处理最后一个 cookie
    std::string lastCookie = cookieStr.substr(start);
    size_t equalPos = lastCookie.find('=');
    if (equalPos != std::string::npos) {
        std::string name = lastCookie.substr(0, equalPos);
        std::string value = lastCookie.substr(equalPos + 1);
        // 去除首尾空格
        name.erase(0, name.find_first_not_of(" "));
        name.erase(name.find_last_not_of(" ") + 1);
        value.erase(0, value.find_first_not_of(" "));
        value.erase(value.find_last_not_of(" ") + 1);
        cookiePairs.emplace_back(std::make_pair(name, value));
    }
}

void processUACInput(const std::string& total_input, std::vector<std::string>& vec, std::string& input_path, std::string& flag, std::string& type) {
    std::stringstream ss(total_input);
    std::string item;
    int count = 0;
    while (ss >> item) {
        if (count == 0)
            type = item;
        else if (count == 1) {
            input_path = item;
        }
        else if (count == 2) {
            flag = item;
        }
        else {
            vec.push_back(item);
        }
        count++;
    }
}
std::string doc_root_str;
std::string UACFilePath;
std::string upload_dir;
//UAP
bool UAP(std::string req_username, std::string req_password)
{
    std::string realpath = doc_root_str + "/userdata/" + req_username;
    fs::path dirPath(realpath);
    if (fs::exists(dirPath))
    {
        realpath.append("/password");
        std::ifstream fin(realpath);
        std::string t;
        fin >> t;
        if (t == req_password)
            return true;
        else
            return false;
    }
    else
        return false;
}

bool is_Repeated(std::string username)
{
    std::string realpath = doc_root_str + "/userdata/" + username;
    fs::path dirPath(realpath);
    if (fs::exists(dirPath))
        return true;
    else
        return false;
}

bool AddUser(std::string username, std::string password)
{
    if (is_Repeated(username))
        return false;
    std::string realpath = doc_root_str + "/userdata/" + username;
    fs::path dirPath(realpath);
    if (fs::create_directory(dirPath)) {
        realpath.append("/password");
        std::ofstream fout(realpath);
        fout << password;
        return true;
    }
    else {
        return false;
    }
}
bool DelUser(std::string username)
{
    std::string realpath = doc_root_str + "/userdata/" + username;
    fs::path dirPath(realpath);
    if (fs::remove_all(dirPath))
        return true;
    else
        return false;
}
//change username or password
bool CUP(const std::string& old_username, const std::string& new_username, const std::string& new_password) {
    std::string realpath = doc_root_str + "/userdata/" + old_username;
    fs::path oldPath(realpath);
    if (!fs::exists(oldPath))
        return false;
    std::string t = doc_root_str + "/userdata/" + new_username;
    fs::path newPath(t);
    fs::rename(realpath, t);
    std::ofstream fout((doc_root_str + "/userdata/" + new_username + "/password"));
    fout << new_password;
    fout.close();
    return true;
}
//UAC
bool UAC(http::request<http::empty_body>& req, string& req_username, string& req_password,std::string &reqpath)
{
    std::string cookie_str;
    auto it = req.find(beast::http::field::cookie);
    if (it == req.end())
    {
		cookie_str = "username=guest;password=guest;";

    }
    else
        cookie_str = it->value();
    http::verb req_type = req.method();
    //GET password ban
    if (reqpath.rfind("/password") == (reqpath.size() - 9) && reqpath.find("/userdata") == 0 && req_type == http::verb::get)
        return false;
    if (reqpath == "/userdata")
        return false;
    std::vector<std::pair<std::string, std::string>> cookiePairs;
    GetCookies(cookie_str, cookiePairs);
    //  std::cout << "Cookie_str:"<<cookie_str << '\n';

    for (const auto& pair : cookiePairs) {
        //      std::cout << pair.first << " " << pair.second << '\n';
        if (pair.first == "username")
            req_username = pair.second;
        else if (pair.first == "password")
            req_password = pair.second;
    }
    //Username & Password Check
    if (!UAP(req_username, req_password))
        return false;

    //UAC
    std::ifstream file;
    file.open(UACFilePath);
    std::string total_input;
    std::string input_path;
    std::vector <std::string> input_usernames;
    std::string flag;
    std::string type;
    while (!file.eof())
    {
        getline(file, total_input);
        //  std::cout << "TTI:\n" << total_input<<'\n';
        processUACInput(total_input, input_usernames, input_path, flag, type);
        if (req_type == http::string_to_verb(type) && reqpath.find(input_path) == 0) //means only at the front,absolute path only.
        {
            // std::cout << reqpath.find(input_path)<<'\n';
            if (flag == "Allow")
            {
                for (std::string t : input_usernames)
                    if (req_username == t)
                    {
                        file.close();
                        return true;
                    }
                return false;
            }
            else
            {
                for (std::string t : input_usernames)
                    if (req_username == t)
                    {
                        file.close();
                        return false;
                    }
                return true;
            }
        }
    }
    return true;
}