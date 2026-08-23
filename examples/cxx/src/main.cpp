// What a C++ program above openkal reaches for, and the one observation that
// settles whether the runtime is really there: an exception thrown across a
// frame and caught.
#include <cstdio>
#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>

static int depth_three(int n) { if (n > 2) throw std::runtime_error("thrown"); return n; }
static int depth_two(int n)   { return depth_three(n) + 1; }
static int depth_one(int n)   { return depth_two(n) + 1; }

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        std::printf("%s: %s\n", ok ? "ok" : "FAIL", what);
        if (!ok) ++failures;
    };

    std::vector<int> v{5, 3, 9, 1};
    std::sort(v.begin(), v.end());
    check(v[0] == 1 && v[3] == 9, "a vector is sorted");

    std::string s = "openkal";
    s += "-llvm-runtime";
    check(s.size() == 20 && s.find("llvm") == 8, "a string is built and searched");

    check(depth_one(1) == 3, "a value returns through three frames");

    bool caught = false;
    try { depth_one(5); } catch (const std::runtime_error& e) { caught = std::string(e.what()) == "thrown"; }
    check(caught, "an exception is thrown across three frames and caught");

    // The unwinder ran; that it ran correctly is what this observes. A frame
    // whose destructor did not run would leave this false.
    struct sentinel { bool* p; ~sentinel() { *p = true; } };
    bool destroyed = false;
    try { sentinel g{&destroyed}; throw 1; } catch (int) {}
    check(destroyed, "a destructor runs while the stack is unwound");

    std::printf("-- failures: %d --\n", failures);
    return failures != 0;
}
