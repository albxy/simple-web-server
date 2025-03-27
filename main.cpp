#include "uploads.h"
#include "impl.h"
#include "users.h"
inline
void
load_server_certificate(boost::asio::ssl::context& ctx)
{
    ctx.set_options(ssl::context::default_workarounds 
        
      | ssl::context::no_sslv2 |
        ssl::context::no_sslv3 |
        ssl::context::no_tlsv1 |
        ssl::context::no_tlsv1_1 //|
       // ssl::context::sslv3_server
        );
    
    ctx.use_certificate_file("server.crt", ssl::context::pem);
    ctx.use_private_key_file("server.key", ssl::context::pem);
}


// Return a response for the given request.
//
// The concrete type of the response message (which depends on the
// request), is type-erased in message_generator.
std::string p4;//
std::string custom_file_path;//
std::string host_name;//

//------------------------------------------------------------------------------


unsigned long long req_number_ = 0; // Request counter
// Handles an HTTP server connection
class session : public std::enable_shared_from_this<session>
{
    ssl::stream<beast::tcp_stream> stream_;
    beast::flat_buffer buffer_;
    std::shared_ptr <std::string const> doc_root_;
    net::steady_timer total_connection_timer_; // Timer for total connection duration
    boost::optional<http::request_parser<http::empty_body>> parser_;
public:
    // Take ownership of the socket
    explicit
        session(
            tcp::socket&& socket,
            ssl::context& ctx,
            std::shared_ptr<std::string const> const& doc_root)
        : stream_(std::move(socket), ctx)
        , doc_root_(doc_root)
        , total_connection_timer_(stream_.get_executor())
    {
    }
    std::string rand_str() {
        
        return to_string(req_number_++);
    }

    // Start the asynchronous operation
    void
        run()
    {
        // Start the connection timer when the first request is received
        total_connection_timer_.expires_after(std::chrono::seconds(3600 * 24));  // Set to desired connection timeout (e.g., 10 seconds)

        net::dispatch(
            stream_.get_executor(),
            beast::bind_front_handler(
                &session::on_run,
                shared_from_this()));
    }

    void
        on_run()
    {
        // Perform the SSL handshake
        stream_.async_handshake(
            ssl::stream_base::server,
            beast::bind_front_handler(
                &session::on_handshake,
                shared_from_this()));
    }

    void
        on_handshake(beast::error_code ec)
    {
        if (ec)
            return fail(ec, "handshake");

        do_read();
    }

    void
        do_read()
    {
		parser_.emplace();
        // Make the request empty before reading,
        // otherwise the operation behavior is undefined.
        // Read a request
        http::async_read_header(stream_, buffer_, *parser_,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));
    }

    void
        on_read(
            beast::error_code ec,
            std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);
        
        if (ec == http::error::end_of_stream)
        {
            return do_close();
        }
        if (ec)
        {
            return fail(ec, "read");
        }        
        
        auto req = parser_.get().get();
        std::string cookie_username, cookie_password;
        decoded_req_target = url_decode(req.target());
        if(!UAC(req,cookie_username,cookie_password,decoded_req_target))
		{
			http::response<http::string_body> res{ http::status::forbidden, req.version() };
			res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
			res.set(http::field::content_type, "text/html");
			res.keep_alive(req.keep_alive());
			res.body() = "Forbidden";
			res.prepare_payload();
			send_response(std::move(res));
		}
        // Log
        logout(req, stream_);
        if (req.method() == http::verb::get)
        {
            std::string path = path_cat(*doc_root_, decoded_req_target);
            if (req.target().back() == '/')
                path.append(custom_file_path);
		//	cout << "path:" << path << endl;
            send_response(handle_get_request(req, p4, path));
        }
        else if (req.method() == http::verb::post)
        {
            if (req.target().find("/files") == 0)
            {
                auto it=req.find(http::field::content_type);
				if (it == req.end())
				{
					http::response<http::string_body> res{ http::status::bad_request, req.version() };
					res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
					res.set(http::field::content_type, "text/html");
					res.keep_alive(req.keep_alive());
					res.body() = "Where is the content_disposition!!!";
					res.prepare_payload();
					send_response(std::move(res));
					return;
				}
                string boundary = it->value();
				size_t boundary_start_pos = boundary.find("boundary=")+9;
                size_t boundary_end_pos = boundary.size();
                if (boundary_start_pos == string::npos )
                {
					http::response<http::string_body> res{ http::status::bad_request, req.version() };
					res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
					res.set(http::field::content_type, "text/html");
					res.keep_alive(req.keep_alive());
					res.body() = "Where is the boundary!!!";
					res.prepare_payload();
					send_response(std::move(res));
					return;
                }
				boundary = boundary.substr(boundary_start_pos,boundary_end_pos-boundary_start_pos);
                http::request_parser<http::buffer_body> parser{ std::move(*parser_) };
                send_response(handle_upload_request(stream_, buffer_, parser,upload_dir + decoded_req_target.substr(6)));
            }
            else
            {
                http::request_parser<http::string_body> parser{ std::move(*parser_) };
                parser.body_limit(1024);
                http::async_read(stream_, buffer_, parser,
					[](beast::error_code ec, std::size_t bytes_transferred)
                    {
                        
						if (ec)
							return fail(ec, "read");                   
                    });
                send_response(handle_post_request(parser.release(), cookie_username, cookie_password));
            }
        }
    }

    void
        send_response(http::message_generator&& msg)
    {
        bool keep_alive = msg.keep_alive();

        // Write the response
        beast::async_write(
            stream_,
            std::move(msg),
            beast::bind_front_handler(
                &session::on_write,
                this->shared_from_this(),
                keep_alive));
    }

    void
        on_write(
            bool keep_alive,
            beast::error_code ec,
            std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
        {
            return fail(ec, "write");
        }
        if (!keep_alive)
        {
            // This means we should close the connection
            return do_close();
        }
        // Read another request    So keep_alive is meaningless now...
     //   do_read();
        do_close();
    }

    void
        do_close()
    {
        // Perform the SSL shutdown
        stream_.async_shutdown(
            beast::bind_front_handler(
                &session::on_shutdown,
                shared_from_this()));
    }

    void
        on_shutdown(beast::error_code ec)
    {
        if (ec)
            return fail(ec, "shutdown");

        // At this point the connection is closed gracefully
    }
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
    net::io_context& ioc_;
    ssl::context& ctx_;
    tcp::acceptor acceptor_;
    std::shared_ptr<std::string const> doc_root_;

public:
    listener(
        net::io_context& ioc,
        ssl::context& ctx,
        tcp::endpoint endpoint,
        std::shared_ptr<std::string const> const& doc_root)
        : ioc_(ioc)
        , ctx_(ctx)
        , acceptor_(ioc)
        , doc_root_(doc_root)
    {
        beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
        {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec)
        {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if (ec)
        {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(
            net::socket_base::max_listen_connections, ec);
        if (ec)
        {
            fail(ec, "listen");
            return;
        }
    }

    // Start accepting incoming connections
    void
        run()
    {
        do_accept();
    }

private:
    void
        do_accept()
    {
        // The new connection gets its own strand
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(
                &listener::on_accept,
                shared_from_this()));
    }

    void
        on_accept(beast::error_code ec, tcp::socket socket)
    {
        if (ec)
        {
            fail(ec, "accept");
            return; // To avoid infinite loop
        }
        else
        {
            // Create the session and run it
            std::make_shared<session>(
                std::move(socket),
                ctx_,
                doc_root_)->run();
        }

        // Accept another connection
        do_accept();
    }
};

//------------------------------------------------------------------------------

int main()
{
    // 
    ptree::ptree config;
    ptree::ini_parser::read_ini("config.ini", config);
    doc_root_str = config.get<std::string>("Server.RootDir");
    auto const address = net::ip::make_address("0.0.0.0");
    auto const port = static_cast<unsigned short>(config.get<int>("Server.Port"));
  //  auto const  port =static_cast <unsigned short> config.get<int>("Server.Port");
    auto const doc_root = std::make_shared<std::string>(doc_root_str);
    auto const threads = std::max<int>(1 , config.get<int>("Server.ThreadNumber"));
    p4 = doc_root_str + config.get<std::string>("Server.404FilePath");
    custom_file_path = config.get<std::string>("Server.CustomFilePath");
    UACFilePath = config.get<std::string>("Server.UACFilePath");
    host_name = config.get<std::string>("Server.Host");
    upload_dir = config.get<std::string>("Server.UploadFilesPath");
//    std::cout << p4;
    // The io_context is required for all I/O
    net::io_context ioc{ threads };

    // The SSL context is required, and holds certificates
    ssl::context ctx{ ssl::context::tlsv12 };

    // This holds the self-signed certificate used by the server
    load_server_certificate(ctx);

    // Create and launch a listening port
    std::make_shared<listener>(
        ioc,
        ctx,
        tcp::endpoint{ address, port },
        doc_root)->run();

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for (auto i = threads - 1; i > 0; --i)
        v.emplace_back(
            [&ioc]
            {
                ioc.run();
            });
    ioc.run();

    return EXIT_SUCCESS;
}