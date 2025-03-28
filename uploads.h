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
#include "others.h"
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace fs = boost::filesystem;  //
namespace ptree = boost::property_tree;
using namespace std;

//  multipart/form-data 
size_t find(const char* buf, std::string& substr, size_t startpos, size_t len)
{
	size_t sbs = substr.size();
	for (size_t i = startpos; i < len - sbs + 1; ++i)
	{
		//	system("pause");
		bool flag = 0;
		for (size_t j = 0; j < sbs; ++j)
		{
			//	////cout<<buf[i+j]<<" "<<substr[j]<<'\n';
			//	Sleep(100);
			if (buf[i + j] != substr[j])
			{
				flag = 1;
				break;
			}
		}
		if (!flag) return i;
	}
	return -1;
}
void copy(char* buf, size_t startpos, size_t len)
{
	for (size_t i = 0; i < len; ++i)
		buf[i] = buf[i + startpos];
}
const size_t def_size = 1024 * 512;

string ParseUploadData(
	ssl::stream<beast::tcp_stream>& stream,
	beast::flat_buffer& buffer,
	http::request_parser<http::buffer_body>& parser,
	std::string upload_folder)
{
	char buf[def_size] = {};
	size_t st = 0;
	size_t buf_size;
	//std::string  temp_file_path = "temp.txt";
	//ifstream fin(temp_file_path.c_str(), ios::binary);

	string boundary;
	//getline(fin, boundary);
	boundary = "--" + boundary;
	size_t round = 0;
	//cout << "READY FOR LOOP\n";
	while (1)
	{
		//	////cout<<"LOOP STARTED"; 
		round++;
		//cout << "round:" << round << '\n';
		string filename;
		//fin.read(buf + st, def_size - st);
		parser.get().body().data = buf+st;
		parser.get().body().size = sizeof(buf)-st;
		http::async_read(stream, buffer, parser,
			[](beast::error_code ec, std::size_t bytes_transferred)
			{
				if (ec == http::error::need_buffer)
					ec = {};
				if (ec)
					return fail(ec, "read");
			});
		buf_size =sizeof(buf) - parser.get().body().size ;
		parser.get().body().more = !parser.is_done();
		cout << "part_of_buf:\n";
		for(int i=0;i<1024;i++)
		{
			cout << buf[i];
		}
		string temp_a = "filename=\"";
		string temp_b = "\"";
		string temp_c = "\r\n\r\n";
		size_t filename_startpos = find(buf, temp_a, 0, buf_size);
		//cout << "fp:" << filename_startpos << '\n';
		if (filename_startpos == -1) return "";
		size_t filename_endpos = find(buf, temp_b, filename_startpos + 10, buf_size - filename_startpos - 10);
		for (size_t i = filename_startpos + 10; i < filename_endpos; ++i)
		{
			filename += buf[i];
		}
		size_t content_startpos = find(buf, temp_c, filename_endpos, buf_size - filename_endpos);
		//cout << "csp:" << content_startpos << '\n';
		copy(buf, content_startpos + 4, buf_size - content_startpos - 4);
		st = buf_size - content_startpos - 4;
		upload_folder=UTF8ToLocal(upload_folder);
		filename=UTF8ToLocal(filename);
		string fullpath = upload_folder + "/" + filename;
		//cout << fullpath << '\n';
		fs::path path(fullpath);
		fs::create_directories(path.parent_path());

		ofstream fout(fullpath.c_str(), ios::binary);
		size_t boundary_size = boundary.size();
		size_t total_gcount = 0;
		//cout << "Ready for loop2\n";
		//system("pause");
		while (1)
		{
			parser.get().body().data = buf + st;
			parser.get().body().size = sizeof(buf) - st;
			http::async_read(stream, buffer, parser,
				[](beast::error_code ec, std::size_t bytes_transferred)
				{
					if (ec == http::error::need_buffer)
						ec = {};
					if (ec)
						return fail(ec, "read");
				});
			buf_size = sizeof(buf) - parser.get().body().size;
			parser.get().body().more = !parser.is_done();
			string temp_d = "\r\n" + boundary;
			size_t content_endpos = find(buf, temp_d, 0, buf_size);
			//cout << "cep:" << content_endpos << '\n';
			////cout << "partofce:" << buf[content_endpos + 2] << '\n';
			//system("pause");
			if (content_endpos == -1)
			{
				fout.write(buf, buf_size - boundary_size - 2);
				total_gcount += (buf_size - boundary_size - 2);
				//cout << "fout.gcount(total):" << total_gcount << '\n';

				copy(buf, buf_size - boundary_size - 2, boundary_size + 2);
				st = boundary_size + 2;
				continue;
			}
			fout.write(buf, content_endpos);
			copy(buf, content_endpos, buf_size - content_endpos);
			st = buf_size - content_endpos;
			//cout << "found:st:" << st << '\n';
			fout.close();
			break;
		}
		//	////cout<<"LOOP ENDED"; 
	}
	//	////cout<<"AT THE ENDOF FUNC";
}
// 
http::message_generator
handle_upload_request(
	ssl::stream<beast::tcp_stream>& stream,
	beast::flat_buffer& buffer,
	http::request_parser<http::buffer_body> &parser,
	const std::string upload_dir) {
	http::response<beast::http::string_body> res;
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/plain");
    res.keep_alive(true);
    res.body() = ParseUploadData(stream,buffer,parser,upload_dir);
    if (res.body().empty())
        res.body() = "Succeeded!";
    res.prepare_payload();
	return res;
}

void logout(http::request<http::buffer_body> &req_, ssl::stream<beast::tcp_stream> &stream_) {
	tcp::endpoint remote_endpoint = stream_.next_layer().socket().remote_endpoint();
	std::string client_ip = remote_endpoint.address().to_string();
	boost::posix_time::ptime currentTime = boost::posix_time::second_clock::local_time();
	ofstream logout("log.log", ios::app);
	boost::posix_time::time_facet* facet = new boost::posix_time::time_facet("%Y-%m-%d %H:%M:%S");
	logout.imbue(std::locale(std::cout.getloc(), facet));

	logout << client_ip << "    " << currentTime << " " << req_.method_string() << " " << req_.target() << '\n';
	logout.close();

}