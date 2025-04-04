#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "impl.h"
#include "uploads.h"
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>



// Handles an HTTP server connection
class session : public std::enable_shared_from_this<session>
{
    ssl::stream<beast::tcp_stream> stream_;
    beast::flat_buffer buffer_;
    std::shared_ptr<std::string const> doc_root_;
    http::request_parser<http::empty_body> parser_;
    char buff[1024];  // Buffer for reading body data

public:
    // Take ownership of the socket
    explicit session(
        tcp::socket&& socket,
        ssl::context& ctx,
        std::shared_ptr<std::string const> const& doc_root)
        : stream_(std::move(socket), ctx)
        , doc_root_(doc_root)
    {
    }

    // Start the asynchronous operation
    void run()
    {
        net::dispatch(
            stream_.get_executor(),
            beast::bind_front_handler(
                &session::on_run,
                shared_from_this()));
    }

    void on_run()
    {
        // Set the timeout.
        beast::get_lowest_layer(stream_).expires_after(
            std::chrono::seconds(3600));

        // Perform the SSL handshake
        stream_.async_handshake(
            ssl::stream_base::server,
            beast::bind_front_handler(
                &session::on_handshake,
                shared_from_this()));
    }

    void on_handshake(beast::error_code ec)
    {
        if (ec)
            return fail(ec, "handshake");

        do_read();
    }

    void do_read()
    {
        // Set the timeout.
        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(3600));

        // Read a request
        http::async_read_header(stream_, buffer_, parser_,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));
    }

    void on_read(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if (ec == http::error::end_of_stream)
            return do_close();

        if (ec)
            return fail(ec, "on read");

        if (parser_.get().method() == http::verb::post &&
            parser_.get().target().find("/files") == 0)
        {

            auto it = parser_.get().find(http::field::content_type);
            if (it == parser_.get().end())
            {
                http::response<http::string_body> res{ http::status::bad_request, parser_.get().version() };
                res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                res.set(http::field::content_type, "text/html");
                res.keep_alive(parser_.get().keep_alive());
                res.body() = "Where is the content_disposition!!!";
                res.prepare_payload();
                send_response(std::move(res));
                return;
            }
            string boundary = it->value();
            size_t boundary_start_pos = boundary.find("boundary=") + 9;
            size_t boundary_end_pos = boundary.size();
            if (boundary_start_pos == string::npos)
            {
                http::response<http::string_body> res{ http::status::bad_request, parser_.get().version() };
                res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                res.set(http::field::content_type, "text/html");
                res.keep_alive(parser_.get().keep_alive());
                res.body() = "Where is the boundary!!!";
                res.prepare_payload();
                send_response(std::move(res));
                return;
            }
            boundary = boundary.substr(boundary_start_pos, boundary_end_pos - boundary_start_pos);
			
			
            string target = parser_.get().target();
            auto parser = std::make_unique<http::request_parser<http::buffer_body>>(std::move(parser_));
            handle_post_file_request(
                stream_,
                buffer_,
                std::move(parser),
                config.upload_path.append(target.substr(6)),
                boundary,
                [self = shared_from_this()](http::message_generator response) {
                    self->send_response(std::move(response));
                });
            /*
            auto* parser_ptr = parser.get();
            http::async_read(
                stream_,
                buffer_,
                *parser_ptr,
                beast::bind_front_handler(
                    &session::next_read,
                    shared_from_this(),
                    std::move(parser)  // Transfer ownership to handler
                )
            );*/

        }
        else
        {
            // Send the response
            send_response(
                request_router(*doc_root_, std::move(parser_.get())));
        }
    }

    void next_read(
        std::unique_ptr<http::request_parser<http::buffer_body>> parser,
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        if (ec && ec != http::error::need_buffer)
        {
            if (ec == http::error::end_of_stream)
                return do_close();
            return fail(ec, "next read");
        }

        // Process the received data
        std::cout << "Received " << bytes_transferred << " bytes\n";
        std::cout << "Buffer content: " << buff << "\n";
        Sleep(1000);
        if (!ec || ec == http::error::need_buffer)
        {
            // More data to read
            parser->get().body().data = buff;
            parser->get().body().size = sizeof(buff);

            auto* parser_ptr = parser.get();
            http::async_read(
                stream_,
                buffer_,
                *parser_ptr,
                beast::bind_front_handler(
                    &session::next_read,
                    shared_from_this(),
                    std::move(parser)
                )
            );
        }
        else
        {
            // All data read
            do_close();
        }
    }

    void send_response(http::message_generator&& msg)
    {
        bool keep_alive = msg.keep_alive();

        beast::async_write(
            stream_,
            std::move(msg),
            beast::bind_front_handler(
                &session::on_write,
                this->shared_from_this(),
                keep_alive));
    }

    void on_write(
        bool keep_alive,
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");

        if (!keep_alive)
        {
            return do_close();
        }

        do_read();
    }

    void do_close()
    {
        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(3600));

        stream_.async_shutdown(
            beast::bind_front_handler(
                &session::on_shutdown,
                shared_from_this()));
    }

    void on_shutdown(beast::error_code ec)
    {
        if (ec)
            return fail(ec, "shutdown");
    }
};

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

int main()
{
    try {
        // The io_context is required for all I/O
        net::io_context ioc{ config.threads };

        // The SSL context is required, and holds certificates
        ssl::context ctx{ ssl::context::tlsv12 };

        // This holds the self-signed certificate used by the server
        load_server_certificate(ctx, config);

        // Create and launch a listening port
        std::make_shared<listener>(
            ioc,
            ctx,
            tcp::endpoint{ net::ip::make_address(config.address), config.port },
            std::make_shared<std::string>(config.doc_root))->run();

        // Run the I/O service on the requested number of threads
        std::vector<std::thread> v;
        v.reserve(config.threads - 1);
        for (auto i = config.threads - 1; i > 0; --i)
            v.emplace_back(
                [&ioc]
                {
                    ioc.run();
                });
        ioc.run();

        // (如果代码运行到这里，说明服务器已停止)
        for (auto& t : v)
            t.join();

        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}