#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <fstream>
#include <regex>
#include <chrono>
#include <ctime>

using tcp = boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;
namespace http = boost::beast::http;
namespace beast = boost::beast;

// URL struct
struct Url {
    std::string scheme;
    std::string host;
    std::string port;
    std::string target;
};

// Simple URL parser
Url parse_url(const std::string& url) {
    std::regex re(R"((http|https)://([^/:]+)(:\d+)?(.*))");
    std::smatch match;

    if (!std::regex_match(url, match, re)) {
        throw std::runtime_error("Invalid URL");
    }

    Url u;
    u.scheme = match[1];
    u.host = match[2];
    u.port = match[3].matched ? match[3].str().substr(1) : (u.scheme == "https" ? "443" : "80");
    u.target = match[4].str().empty() ? "/" : match[4].str();

    return u;
}

// Print current timestamp
std::string timestamp() {
    auto now = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return buf;
}

// HTTPS certificate info
void print_cert(ssl::stream<tcp::socket>& stream) {
    X509* cert = SSL_get_peer_certificate(stream.native_handle());
    if (!cert) return;

    char subject[256], issuer[256];
    X509_NAME_oneline(X509_get_subject_name(cert), subject, 256);
    X509_NAME_oneline(X509_get_issuer_name(cert), issuer, 256);

    std::cout << "Certificate Subject: " << subject << "\n";
    std::cout << "Certificate Issuer: " << issuer << "\n";

    X509_free(cert);
}

// Perform HTTP/HTTPS request
std::tuple<std::string, size_t> do_request(boost::asio::io_context& ioc,
                                           ssl::context& ctx,
                                           const Url& url,
                                           const std::string& output,
                                           bool& redirected) {
    beast::flat_buffer buffer;

    http::response<http::vector_body<char>> res;

    if (url.scheme == "https") {
        ssl::stream<tcp::socket> stream(ioc, ctx);
        tcp::resolver resolver(ioc);

        auto results = resolver.resolve(url.host, url.port);
        boost::asio::connect(stream.next_layer(), results);

        stream.handshake(ssl::stream_base::client);
        print_cert(stream);

        http::request<http::empty_body> req{http::verb::get, url.target, 11};
        req.set(http::field::host, url.host);
        req.set(http::field::user_agent, "mycurl");

        http::write(stream, req);
        http::read(stream, buffer, res);

    } else {
        tcp::resolver resolver(ioc);
        tcp::socket socket(ioc);

        auto results = resolver.resolve(url.host, url.port);
        boost::asio::connect(socket, results);

        http::request<http::empty_body> req{http::verb::get, url.target, 11};
        req.set(http::field::host, url.host);
        req.set(http::field::user_agent, "mycurl");

        http::write(socket, req);
        http::read(socket, buffer, res);
    }

    // Print headers
    std::cout << res.base() << "\n";

    // Handle redirect
    if (res.result_int() >= 300 && res.result_int() < 400) {
        if (res.base().find("Location") != res.base().end()) {
            redirected = true;
            std::string loc = std::string(res.base()["Location"]);
            std::cout << "Redirected to: " << loc << "\n";
            return {loc, 0};
        }
    }

    // Save body if needed
    size_t size = res.body().size();

    if (!output.empty()) {
        std::ofstream out(output, std::ios::binary);
        out.write(res.body().data(), size);
    }

    return {"", size};
}


// Entry point
int main(int argc, char* argv[]) {
    std::string output;
    std::string url;

    // Parse args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" || arg == "--output") {
            output = argv[++i];
        } else {
            url = arg;
        }
    }

    if (url.empty()) {
        std::cerr << "Usage: ./mycurl [-o file] url\n";
        return 1;
    }

    boost::asio::io_context ioc;
    ssl::context ctx(ssl::context::sslv23_client);
    ctx.set_default_verify_paths();

    std::string current_url = url;
    size_t body_size = 0;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 10; ++i) {
        Url u = parse_url(current_url);

        bool redirected = false;
        auto [next_url, size] = do_request(ioc, ctx, u, output, redirected);

        if (redirected) {
            current_url = next_url;
            continue;
        } else {
            body_size = size;
            break;
        }
    }

    auto end = std::chrono::steady_clock::now();
    double duration = std::chrono::duration<double>(end - start).count();

    double mbps = (body_size * 8.0) / (duration * 1000000.0);

    std::cout << timestamp() << " "
              << current_url << " "
              << body_size << " [bytes] "
              << duration << " [s] "
              << mbps << " [Mbps]\n";

    return 0;
}
