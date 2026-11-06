// The C++ runtime in an ok image: each line is one observation, and the last
// counts the ones that did not hold.
import std;

namespace {
int failures = 0;
void check(bool held, std::string_view what) {
    std::println("{} {}", held ? "  ok  " : "  FAIL", what);
    if (!held) ++failures;
}
thread_local int tl = 7;
std::atomic<int> tls_dtor_ran{0};
struct TlsProbe { int v; ~TlsProbe() { tls_dtor_ran.store(v); } };
}

int main(int argc, char** argv) {
    check(argc >= 1 && std::string_view(argv[0]).ends_with(".oke"), "the image is told its own name");

    bool caught = false;
    try { throw std::runtime_error("thrown"); }
    catch (const std::exception& e) { caught = std::string_view(e.what()) == "thrown"; }
    check(caught, "an exception is caught by its base class");

    int seen = 0;
    std::thread t([&] { seen = tl; tl = 99; });
    t.join();
    check(seen == 7 && tl == 7, "a thread has its own thread-local variable");

    // A thread_local with a destructor needs `__cxa_thread_atexit`, which the
    // image's runtime provides; the image would not link without it.
    std::thread d([] { thread_local TlsProbe p{7}; (void)p; });
    d.join();
    check(tls_dtor_ran.load() == 7, "a thread_local's destructor runs when its thread ends");

    std::map<std::string, int> m{{"a", 1}, {"b", 2}};
    std::vector<int> v(100);
    std::iota(v.begin(), v.end(), 1);
    check(m.at("b") == 2 && std::accumulate(v.begin(), v.end(), 0) == 5050, "the containers and algorithms");

    check(std::format("{:>6.2f}", 3.14159) == "  3.14", "formatting a floating-point value");

    // --- names beneath a volume (openkal-llvm-runtime#64) -------------------
    //
    // The C library names a file on a system with volumes `D:/...` and takes
    // `X:/` as a root. The same name is absolute to std::filesystem, and the
    // working directory is absolute on every system.
    {
        namespace fs = std::filesystem;
        const fs::path v = "D:/a/b.txt";
        check(v.is_absolute() && v.root_name() == "D:" && v.root_directory() == "/", "`D:/a/b.txt' has a root name and a root directory");
        check(v.relative_path() == "a/b.txt" && v.parent_path() == "D:/a" && v.filename() == "b.txt", "and decomposes beneath it");
        std::vector<std::string> forward, backward;
        for (const auto& e : v) forward.push_back(e.string());
        for (auto it = v.end(); it != v.begin();) backward.insert(backward.begin(), (--it)->string());
        check(forward == std::vector<std::string>{"D:", "/", "a", "b.txt"} && backward == forward, "its elements are the same read either way");
        check(!fs::path("D:").is_absolute() && fs::path("D:").root_name().empty(), "`D:' without a separator is an ordinary name");
        check(!fs::path("D:x").is_absolute(), "`D:x' is relative");
        check((fs::path("x") / "D:/y") == "D:/y" && fs::path("x").append(std::string("D:/y")) == "D:/y", "appending an absolute volume name replaces the path");
        check(fs::path("D:/a/b").lexically_relative("D:/a") == "b" && fs::path("/a/b").is_absolute(), "a name relative to a volume, and `/' unchanged");
        std::error_code cwd_ec;
        const fs::path cwd = fs::current_path(cwd_ec);
        check(!cwd_ec && cwd.is_absolute(), "the working directory is absolute");
        check(fs::absolute(cwd, cwd_ec) == cwd && !cwd_ec, "absolute() of it is itself");
        const fs::path can = fs::canonical(cwd, cwd_ec);
        check(!cwd_ec && can.is_absolute(), "canonical() of it succeeds");
    }

    std::println("-- failures: {} --", failures);
    return failures ? 1 : 0;
}
