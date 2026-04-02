// =============================================================================
// main.cpp – demonstrates each supported library with a minimal working sample.
//
// Sections:
//   1. Boost.ProgramOptions  – command-line argument parsing
//   2. Boost.JSON            – parse, inspect, and serialize JSON
//   3. Boost.Asio            – async timer (event-loop / io_context pattern)
//   4. Boost.Beast           – synchronous HTTP GET over plain TCP
//   5. std::filesystem       – enumerate files in the working directory
//   6. std::regex            – search for an IP address pattern in a string
//   7. std::thread / future  – background task with std::async
//   8. FP (std::transform /  – map, filter, reduce over a vector
//          std::copy_if /
//          std::reduce)
//   9. Embedded resource     – binary file linked into the executable at build
//                              time via llvm-objcopy; accessed through linker
//                              symbols with no file I/O at runtime
//  10. spdlog                 – structured logging with coloured stdout sink
//                              and a size-based rotating file sink; oldest
//                              files deleted automatically when limit is reached
//  11. Boost.URL              – parse, inspect, and mutate URIs
//  12. Boost.UUID             – generate random (v4) and name-based (v5) UUIDs
//  13. Boost.Process v2       – launch a child process, capture its stdout
//  14. Boost.Stacktrace       – capture and print the current call stack
//  15. Howard Hinnant's Date  – calendar date arithmetic on top of <chrono>
//                              (core library only; no IANA timezone data needed)
//  16. Vince's CSV Parser     – parse CSV data, access fields by column name
// =============================================================================

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <numeric>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <boost/program_options.hpp>
#include <boost/url.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/process/v2.hpp>
#include <boost/stacktrace.hpp>
#include <date/date.h>
#include <csv.hpp>

#include "math_utils.h"
#include "embedded_resource.h"
#include "logger.h"

namespace fs  = std::filesystem;
namespace po  = boost::program_options;
namespace net = boost::asio;
using     tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace json  = boost::json;

// -----------------------------------------------------------------------------
// 1. Boost.ProgramOptions
// -----------------------------------------------------------------------------
static po::variables_map parse_options(int argc, char* argv[]) {
    po::options_description desc("Options");
    desc.add_options()
        ("help,h",  "show this help message")
        ("host",    po::value<std::string>()->default_value("localhost"),
                    "server hostname")
        ("port,p",  po::value<int>()->default_value(8080),
                    "server port");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        std::exit(0);
    }
    return vm;
}

// -----------------------------------------------------------------------------
// 2. Boost.JSON
// -----------------------------------------------------------------------------
static void demo_json() {
    std::cout << "\n--- Boost.JSON ---\n";

    // Parse
    const std::string raw = R"({
        "service": "example-api",
        "version": 3,
        "features": ["async", "json", "http"]
    })";
    json::value doc = json::parse(raw);
    const json::object& obj = doc.as_object();

    std::cout << "service : " << obj.at("service").as_string() << "\n";
    std::cout << "version : " << obj.at("version").as_int64()  << "\n";
    std::cout << "features: ";
    for (const auto& f : obj.at("features").as_array()) {
        std::cout << f.as_string() << " ";
    }
    std::cout << "\n";

    // Serialize
    json::object response;
    response["status"] = "ok";
    response["result"] = add(6, 7);     // calls production code from math_utils
    std::cout << "serialized: " << json::serialize(response) << "\n";
}

// -----------------------------------------------------------------------------
// 3. Boost.Asio – async timer
// -----------------------------------------------------------------------------
static void demo_asio_timer() {
    std::cout << "\n--- Boost.Asio: async timer ---\n";

    net::io_context ioc;
    net::steady_timer timer(ioc, std::chrono::milliseconds(200));

    timer.async_wait([](const boost::system::error_code& ec) {
        if (!ec) {
            std::cout << "timer fired (200 ms)\n";
        }
    });

    // Run the event loop until all async work is complete
    ioc.run();
}

// -----------------------------------------------------------------------------
// 4. Boost.Beast – synchronous HTTP GET
// -----------------------------------------------------------------------------
static void demo_beast_http() {
    std::cout << "\n--- Boost.Beast: HTTP GET example.com ---\n";
    try {
        net::io_context ioc;
        tcp::resolver   resolver(ioc);
        beast::tcp_stream stream(ioc);

        // Resolve and connect
        stream.connect(resolver.resolve("example.com", "80"));

        // Build the request
        http::request<http::string_body> req{http::verb::get, "/", 11};
        req.set(http::field::host,       "example.com");
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.set(http::field::connection, "close");
        http::write(stream, req);

        // Read the response
        beast::flat_buffer                  buf;
        http::response<http::dynamic_body>  res;
        http::read(stream, buf, res);

        std::cout << "status : " << res.result_int() << " " << res.reason() << "\n";
        std::cout << "body   : "
                  << beast::buffers_to_string(res.body().data()).size()
                  << " bytes\n";

        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

    } catch (const std::exception& e) {
        std::cout << "skipped (no network or DNS): " << e.what() << "\n";
    }
}

// -----------------------------------------------------------------------------
// 5. std::filesystem
// -----------------------------------------------------------------------------
static void demo_filesystem() {
    std::cout << "\n--- std::filesystem ---\n";

    const fs::path cwd = fs::current_path();
    std::cout << "working directory: " << cwd << "\n";
    std::cout << "contents:\n";

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(cwd, ec)) {
        const char* kind = entry.is_directory() ? "DIR " : "FILE";
        std::cout << "  [" << kind << "] "
                  << entry.path().filename().string() << "\n";
    }
}

// -----------------------------------------------------------------------------
// 6. std::regex
// -----------------------------------------------------------------------------
static void demo_regex() {
    std::cout << "\n--- std::regex ---\n";

    const std::string text = "connect to 192.168.1.100 on port 8080";
    const std::regex  ip_pattern(R"(\d{1,3}(?:\.\d{1,3}){3})");

    std::smatch m;
    if (std::regex_search(text, m, ip_pattern)) {
        std::cout << "found IP address: " << m[0] << "\n";
    }

    // Iterate over all matches
    auto begin = std::sregex_iterator(text.begin(), text.end(),
                                      std::regex(R"(\d+)"));
    auto end   = std::sregex_iterator{};
    std::cout << "all numbers in text: ";
    for (auto it = begin; it != end; ++it) {
        std::cout << (*it)[0] << " ";
    }
    std::cout << "\n";
}

// -----------------------------------------------------------------------------
// 7. std::thread / std::future
// -----------------------------------------------------------------------------
static void demo_thread_future() {
    std::cout << "\n--- std::thread / std::future ---\n";

    // std::async fires the lambda on a thread-pool thread
    auto fut = std::async(std::launch::async, []() -> int {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return add(100, 200);   // calls production code
    });

    // Do other work here while the async task runs ...
    std::cout << "waiting for async result...\n";

    std::cout << "async result: " << fut.get() << "\n";

    // Explicit std::thread example
    std::thread t([]() {
        std::cout << "background thread id: "
                  << std::this_thread::get_id() << "\n";
    });
    t.join();
}

// -----------------------------------------------------------------------------
// 8. FP – map / filter / reduce
// -----------------------------------------------------------------------------
static void demo_fp() {
    std::cout << "\n--- FP: map / filter / reduce ---\n";

    const std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // map: square every element
    std::vector<int> squared;
    squared.reserve(numbers.size());
    std::transform(numbers.begin(), numbers.end(),
                   std::back_inserter(squared),
                   [](int n) { return n * n; });

    std::cout << "squared: ";
    for (int n : squared) { std::cout << n << " "; }
    std::cout << "\n";

    // filter: keep only even numbers
    std::vector<int> evens;
    std::copy_if(numbers.begin(), numbers.end(),
                 std::back_inserter(evens),
                 [](int n) { return n % 2 == 0; });

    std::cout << "evens  : ";
    for (int n : evens) { std::cout << n << " "; }
    std::cout << "\n";

    // reduce: sum all elements (std::reduce is C++17, supports parallel policy)
    const int total = std::reduce(numbers.begin(), numbers.end(), 0);
    std::cout << "sum    : " << total << "\n";

    // chained: sum of squares of even numbers
    std::vector<int> even_squares;
    std::copy_if(squared.begin(), squared.end(),
                 std::back_inserter(even_squares),
                 [](int n) {
                     const int root = static_cast<int>(std::sqrt(n));
                     return root * root == n && root % 2 == 0;
                 });
    const int even_square_sum = std::reduce(even_squares.begin(),
                                            even_squares.end(), 0);
    std::cout << "sum of squares of evens: " << even_square_sum << "\n";
}

// -----------------------------------------------------------------------------
// 9. Embedded resource – binary file linked in at build time via llvm-objcopy
// -----------------------------------------------------------------------------
static void demo_embedded_resource() {
    std::cout << "\n--- Embedded resource (llvm-objcopy, no file I/O) ---\n";

    // get_embedded_sample_json() returns a string_view directly into the
    // executable's read-only data segment – no heap allocation, no file open.
    const std::string_view raw = get_embedded_sample_json();
    std::cout << "embedded size  : " << raw.size() << " bytes\n";

    // Parse the embedded bytes as JSON using Boost.JSON (already linked in).
    const json::value  doc = json::parse(raw);
    const json::object& obj = doc.as_object();

    std::cout << "application    : " << obj.at("application").as_string() << "\n";
    std::cout << "description    : " << obj.at("description").as_string() << "\n";

    const json::array& features =
        obj.at("settings").as_object().at("features").as_array();
    std::cout << "features       : ";
    for (const auto& f : features) {
        std::cout << f.as_string() << " ";
    }
    std::cout << "\n";
}

// -----------------------------------------------------------------------------
// 10. spdlog – structured logging with multiple sinks and log rotation
// -----------------------------------------------------------------------------
static void demo_spdlog() {
    std::cout << "\n--- spdlog: structured logging + rotating file sink ---\n";

    // Write logs to a temp subdirectory so the demo works from any cwd.
    const fs::path log_dir = fs::temp_directory_path() / "cpp_app_demo_logs";
    auto log = setup_logger(log_dir);

    std::cout << "log directory  : " << log_dir.string() << "\n";
    std::cout << "rotation policy: 5 MB max per file, 3 files kept\n\n";

    // All four severity levels.  The pattern written to both sinks is:
    //   [2026-04-02 09:00:00.123] [INFO ]  message
    log->debug("debug: detailed diagnostic – usually disabled in production");
    log->info ("info:  server listening on port {}", 8080);
    log->warn ("warn:  disk usage at {}%, consider archiving old data", 87);
    log->error("error: failed to open config file, falling back to defaults");

    log->flush();
    spdlog::drop("app");  // deregister so the demo can be re-run in the same process
}

// -----------------------------------------------------------------------------
// 11. Boost.URL – parse, inspect, and mutate URIs
// -----------------------------------------------------------------------------
static void demo_boost_url() {
    std::cout << "\n--- Boost.URL ---\n";
    namespace urls = boost::urls;

    // Parse to an immutable view (zero-copy; input string must remain alive)
    const urls::url_view uv = urls::parse_uri(
        "https://api.example.com:8443/v2/users?page=1&limit=50#results").value();

    std::cout << "scheme   : " << uv.scheme()   << "\n";
    std::cout << "host     : " << uv.host()     << "\n";
    std::cout << "port     : " << uv.port()     << "\n";
    std::cout << "path     : " << uv.path()     << "\n";
    std::cout << "query    : " << uv.query()    << "\n";
    std::cout << "fragment : " << uv.fragment() << "\n";

    // Mutable url for building / modifying
    urls::url u(uv);
    u.set_path("/v3/users");
    u.set_query("page=2&limit=25");
    std::cout << "modified : " << u << "\n";
}

// -----------------------------------------------------------------------------
// 12. Boost.UUID – random (v4) and name-based (v5) UUIDs
// -----------------------------------------------------------------------------
static void demo_boost_uuid() {
    std::cout << "\n--- Boost.UUID ---\n";

    // v4: cryptographically random
    boost::uuids::random_generator rgen;
    const auto id1 = rgen();
    const auto id2 = rgen();
    std::cout << "random uuid 1  : " << id1 << "\n";
    std::cout << "random uuid 2  : " << id2 << "\n";
    std::cout << "are equal      : " << (id1 == id2 ? "yes" : "no") << "\n";

    // v5: name-based (SHA-1) – identical input always yields identical UUID
    boost::uuids::name_generator_sha1 ngen(boost::uuids::ns::url());
    const auto id3 = ngen("https://example.com");
    const auto id4 = ngen("https://example.com");
    std::cout << "name-based     : " << id3 << "\n";
    std::cout << "reproducible   : " << (id3 == id4 ? "yes" : "no") << "\n";

    // String round-trip
    const std::string str = boost::uuids::to_string(id1);
    const auto        id5 = boost::uuids::string_generator()(str);
    std::cout << "string form    : " << str << "\n";
    std::cout << "round-trip ok  : " << (id1 == id5 ? "yes" : "no") << "\n";
}

// -----------------------------------------------------------------------------
// 13. Boost.Process v2 – launch a child process and capture its stdout
// -----------------------------------------------------------------------------
static void demo_boost_process() {
    std::cout << "\n--- Boost.Process v2 ---\n";
    namespace bp = boost::process::v2;

    // cmake must be installed on any machine that can build this project.
    const auto cmake_exe = bp::environment::find_executable("cmake");
    if (cmake_exe.empty()) {
        std::cout << "cmake not found in PATH; skipping\n";
        return;
    }
    std::cout << "cmake path     : " << cmake_exe.string() << "\n";

    net::io_context ioc;
    boost::asio::readable_pipe rp{ioc};

    // Redirect child stdout to the pipe; stdin and stderr are inherited.
    bp::process_stdio stdio;
    stdio.out = rp;

    bp::process proc(ioc, cmake_exe, {"--version"}, stdio);

    // Synchronous read until the child closes its end of the pipe (EOF).
    std::string output;
    boost::system::error_code ec;
    boost::asio::read(rp, boost::asio::dynamic_buffer(output), ec);
    // ec == boost::asio::error::eof here – expected, not an error.

    const int code = proc.wait();

    // Print only the first line ("cmake version X.Y.Z")
    const auto nl = output.find('\n');
    std::cout << "output         : "
              << output.substr(0, nl != std::string::npos ? nl : output.size())
              << "\n";
    std::cout << "exit code      : " << code << "\n";
}

// -----------------------------------------------------------------------------
// 14. Boost.Stacktrace – capture and print the current call stack
// -----------------------------------------------------------------------------
static void demo_stacktrace() {
    std::cout << "\n--- Boost.Stacktrace ---\n";

    // Captures the call stack at this point.
    // Built with BOOST_STACKTRACE_USE_BASIC so no debug-info files are needed;
    // each frame shows a raw address.  For human-readable names rebuild with:
    //   Linux  : Boost::stacktrace_addr2line + BOOST_STACKTRACE_USE_ADDR2LINE
    //   Windows: Boost::stacktrace_windbg    + BOOST_STACKTRACE_USE_WINDBG
    const boost::stacktrace::stacktrace st;
    std::cout << "frames captured: " << st.size() << "\n";

    const std::size_t show = std::min(st.size(), std::size_t{5});
    for (std::size_t i = 0; i < show; ++i) {
        std::cout << "  [" << i << "] " << st[i] << "\n";
    }
    if (st.size() > show) {
        std::cout << "  ... (" << (st.size() - show) << " more frames)\n";
    }
}

// -----------------------------------------------------------------------------
// 15. Howard Hinnant's Date library – calendar arithmetic on top of <chrono>
//     (core library only; no IANA timezone database required)
// -----------------------------------------------------------------------------
static void demo_date() {
    std::cout << "\n--- Howard Hinnant's Date library ---\n";
    using namespace date;
    using namespace std::chrono;

    // today as a calendar date
    const auto today = floor<days>(system_clock::now());
    const auto ymd   = year_month_day{today};

    std::cout << "today          : " << ymd << "\n";
    std::cout << "year           : " << static_cast<int>(ymd.year())      << "\n";
    std::cout << "month          : " << static_cast<unsigned>(ymd.month()) << "\n";
    std::cout << "day            : " << static_cast<unsigned>(ymd.day())   << "\n";
    std::cout << "day of week    : " << weekday{today} << "\n";

    // arithmetic
    std::cout << "in 7 days      : " << year_month_day{sys_days{today} + days{7}}  << "\n";
    std::cout << "in 1 year      : " << (ymd.year() + years{1}) / ymd.month() / ymd.day() << "\n";

    // days remaining until 1 Jan next year
    const auto jan1_next  = (ymd.year() + years{1}) / January / 1;
    const auto days_left  = (sys_days{jan1_next} - sys_days{today}).count();
    std::cout << "days to Jan 1  : " << days_left << "\n";
}

// -----------------------------------------------------------------------------
// 16. Vince's CSV Parser – parse CSV, access fields by column name
// -----------------------------------------------------------------------------
static void demo_csv() {
    std::cout << "\n--- Vince's CSV Parser ---\n";

    // Inline data keeps the demo self-contained (no file path dependency).
    // To read a file instead: csv::CSVReader reader("path/to/file.csv");
    const std::string csv_data =
        "name,age,city\n"
        "Alice,30,New York\n"
        "Bob,25,London\n"
        "Charlie,35,Tokyo\n";

    auto reader = csv::parse(csv_data);

    std::cout << "columns        : ";
    for (const auto& col : reader.get_col_names()) {
        std::cout << col << "  ";
    }
    std::cout << "\n";

    for (auto& row : reader) {
        std::cout << "  "
                  << row["name"].get<std::string>() << ", "
                  << "age " << row["age"].get<int>() << ", "
                  << row["city"].get<std::string>()  << "\n";
    }
}

// =============================================================================
// main
// =============================================================================
int main(int argc, char* argv[]) {
    std::cout << "=== Cross-platform C++ app ===\n";

#if defined(_WIN32)
    std::cout << "platform: Windows\n";
#elif defined(__APPLE__)
    std::cout << "platform: macOS\n";
#elif defined(__linux__)
    std::cout << "platform: Linux\n";
#else
    std::cout << "platform: unknown\n";
#endif

    const auto vm = parse_options(argc, argv);
    std::cout << "host: " << vm["host"].as<std::string>()
              << "  port: " << vm["port"].as<int>() << "\n";

    demo_json();
    demo_asio_timer();
    demo_beast_http();
    demo_filesystem();
    demo_regex();
    demo_thread_future();
    demo_fp();
    demo_embedded_resource();
    demo_spdlog();
    demo_boost_url();
    demo_boost_uuid();
    demo_boost_process();
    demo_stacktrace();
    demo_date();
    demo_csv();

    return 0;
}
