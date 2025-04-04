#pragma once
#include "others.h"
namespace fs = boost::filesystem;
using namespace std;
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
inline void copy(char* buf, size_t startpos, size_t len)
{
	for (size_t i = 0; i < len; ++i)
		buf[i] = buf[i + startpos];
}
class ParseUploadData : public std::enable_shared_from_this<ParseUploadData>
{
private:
	ssl::stream<beast::tcp_stream>& stream;
	beast::flat_buffer& buffer;
	std::unique_ptr<http::request_parser<http::buffer_body>> parser;
	std::string upload_folder;
	std::string boundary;
	char buf[1024 * 512] = {};
	size_t st = 0;
	size_t buf_size;
	string filename;
	ofstream fout;
	size_t boundary_size;
	string fullpath;
	std::function<void()> completion_handler_;
	// 私有构造函数
    ParseUploadData(
        ssl::stream<beast::tcp_stream>& stream_,
        beast::flat_buffer& buffer_,
        std::unique_ptr<http::request_parser<http::buffer_body>> parser_,
        std::string upload_folder_,
        std::string boundary_
    ) : stream(stream_), buffer(buffer_), parser(std::move(parser_)),
        upload_folder(std::move(upload_folder_)), boundary(std::move(boundary_))
    {
        boundary_size = boundary.size();
    }
public:
	ParseUploadData() = delete;
	void set_completion_handler(std::function<void()> handler) {
		completion_handler_ = std::move(handler);
	}

	static std::shared_ptr<ParseUploadData> create(
		ssl::stream<beast::tcp_stream>& stream,
		beast::flat_buffer& buffer,
		std::unique_ptr<http::request_parser<http::buffer_body>> parser,
		std::string upload_folder,
		std::string boundary
	) {
		return std::shared_ptr<ParseUploadData>(
			new ParseUploadData(stream, buffer, std::move(parser),
				std::move(upload_folder), std::move(boundary)));
	}
	void start()
	{
		
		parser->get().body().data = buf + st;
		parser->get().body().size = sizeof(buf) - st;
	
		http::async_read(stream, buffer, *parser,
			beast::bind_front_handler(
				&ParseUploadData::ready_for_part, shared_from_this()
				));

	}
	inline void ready_for_part(beast::error_code ec, size_t bytes_transferred)
	{
		if (ec == http::error::need_buffer)
		{
			ec = {};
		}
		if (ec)
		{
			if (completion_handler_)
				completion_handler_();
			return fail(ec,"ready_f_p");
		}
		
		buf_size = sizeof(buf) - parser->get().body().size;
		parser->get().body().more = !parser->is_done();
		string temp_a = "filename=\"";
		string temp_b = "\"";
		string temp_c = "\r\n\r\n";
		size_t filename_startpos = find(buf, temp_a, 0, buf_size);
		if (filename_startpos == -1)
		{
			if (completion_handler_)
				completion_handler_();
			return;
		}
		size_t filename_endpos = find(buf, temp_b, filename_startpos + 10, buf_size - filename_startpos - 10);
		if (filename_endpos == -1)
		{
			if (completion_handler_)
				completion_handler_();
			return;
		}
		for (size_t i = filename_startpos + 10; i < filename_endpos; ++i)
		{
			filename += buf[i];
		}
		size_t content_startpos = find(buf, temp_c, filename_endpos, buf_size - filename_endpos);
		if (content_startpos == -1)
		{
			if (completion_handler_)
				completion_handler_();
			return;
		}
		copy(buf, content_startpos + 4, buf_size - content_startpos - 4);
		st = buf_size - content_startpos - 4;

		filename = UTF8ToLocal(filename);
		fullpath = upload_folder + "/" + filename;
		//cout << "fullpath:" << fullpath << '\n';
		fs::path path(fullpath);
		fs::create_directories(path.parent_path());
		fout.open(fullpath.c_str(), ios::binary);
		on_part_read(ec,bytes_transferred);
	}
	inline void on_part_read(beast::error_code ec, size_t bytes_transferred)
	{
		if (ec == http::error::need_buffer)
		{
			ec = {};
		}
		if (ec)
		{
			if (completion_handler_)
				completion_handler_();
			return fail(ec, "on_part_read");
		}
		
		parser->get().body().data = buf + st;
		parser->get().body().size = sizeof(buf) - st;
		
		http::async_read(stream, buffer, *parser,
			beast::bind_front_handler(
				&ParseUploadData::on_part,shared_from_this()
				));
	}
	inline void on_part(beast::error_code ec, size_t bytes_transferred)
	{
		if (ec == http::error::need_buffer)
		{
			ec = {};
		}
		if (ec)
		{
			if (completion_handler_)
				completion_handler_();
			return fail(ec, "on_part");
		}
	//	cout << "on_part_read's bt:" << bytes_transferred << '\n';
		buf_size = sizeof(buf) - parser->get().body().size;
		parser->get().body().more = !parser->is_done();
	//	system("pause");
		string temp_d = "\r\n" + boundary;
		size_t content_endpos = find(buf, temp_d, 0, buf_size);
		if (content_endpos == -1)
		{
			fout.write(buf, buf_size - boundary_size - 2);
		//	cout << "fout.gcount():" << buf_size - boundary_size - 2 << '\n';
		//	cout << "part of buf:" << buf << '\n';
			copy(buf, buf_size - boundary_size - 2, boundary_size + 2);
			st = boundary_size + 2;
			on_part_read(ec,bytes_transferred);
		}
		else
		{
			fout.write(buf, content_endpos);
		//	cout << "content_ended:" << buf_size - boundary_size - 2 << '\n';
			copy(buf, content_endpos, buf_size - content_endpos);
			st = buf_size - content_endpos;
			fout.close();
			if (!parser->is_done())
				start();
			else
				if (completion_handler_)
					completion_handler_();
		}
	}
};

void handle_post_file_request(
	ssl::stream<beast::tcp_stream>& stream,
	beast::flat_buffer& buffer,
	std::unique_ptr<http::request_parser<http::buffer_body>> parser,
	std::string upload_dir,
	string boundary,
	std::function<void(http::message_generator)> completion_handler)
{
	boundary = "--" + boundary;

	auto ps = ParseUploadData::create(
		stream,
		buffer,
		std::move(parser),
		upload_dir,
		boundary
	);

	// 设置完成回调
	ps->set_completion_handler([completion_handler]() {
		http::response<http::string_body> res;
		res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
		res.set(http::field::content_type, "text/plain");
		res.keep_alive(true);
		res.body() = "Succeeded!";
		res.prepare_payload();
		completion_handler(std::move(res));
		});

	ps->start();
}