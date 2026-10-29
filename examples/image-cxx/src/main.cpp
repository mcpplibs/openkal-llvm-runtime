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

    std::println("-- failures: {} --", failures);
    return failures ? 1 : 0;
}
