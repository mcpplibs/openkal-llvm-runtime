// What a C++ program above openkal reaches for, and the one observation that
// settles whether the runtime is really there: an exception thrown across a
// frame and caught.
//
// ⭐ AND THE PARTS OF THE STANDARD LIBRARY WHOSE AVAILABILITY IS A STATEMENT
// ABOUT THE ENVIRONMENT BENEATH. `__config_site` declares
// `_LIBCPP_HAS_FILESYSTEM 1` and `_LIBCPP_HAS_RANDOM_DEVICE 1` for a hosted
// row; those declarations are this package's claim about openkal-musl, and a
// claim that drifts from what the port provides does not fail to build and does
// not fail to link --- it produces a program that takes a path the environment
// cannot support and reports nothing. The same shape as `_LIBCPP_HAS_TERMINAL`,
// which is what the workflow's reconciliation step exists because of.
#include <cstdio>
#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <filesystem>
#include <random>
#include <system_error>

// ⭐ THREE NAMES A PROGRAM ABOVE THIS STACK MAY USE, ASSERTED BY COMPILING.
//
// musl's INTERNAL header overlay defines `hidden`, `weak` and `weak_alias` as
// macros that mean something only to musl's own sources, and openkal-musl used
// to publish the path it is built from --- so a program above it could not
// declare any of the three (mcpplibs/openkal-musl#13). This is the C++ half of
// that criterion, and it belongs here rather than there: the report came from a
// C++23 workspace, and `hidden` in C++ was given C LINKAGE rather than emptied,
// which is a different failure from the C one and would not be caught by it.
static int hidden = 7;
static int weak = 11;
struct weak_alias { int value; };

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

    check(hidden + weak == 18 && weak_alias{3}.value == 3,
          "hidden, weak and weak_alias are the program's own identifiers");

    // --- std::filesystem, which is the C++ face of openkal.fs ---------------

    namespace fs = std::filesystem;
    const fs::path dir = "cxx-probe.d";
    std::error_code ec;

    fs::remove_all(dir, ec);
    check(fs::create_directory(dir, ec) && !ec, "a directory is created");

    {
        std::FILE* f = std::fopen((dir / "a.txt").c_str(), "w");
        check(f != nullptr, "a file is created inside it");
        if (f) { std::fputs("0123456789", f); std::fclose(f); }
    }

    check(fs::exists(dir / "a.txt", ec) && !ec, "the file is found by name");
    check(fs::file_size(dir / "a.txt", ec) == 10 && !ec, "its size is reported");

    int entries = 0;
    for (const auto& e : fs::directory_iterator(dir, ec)) { (void)e; ++entries; }
    check(entries == 1 && !ec, "the directory enumerates exactly what is in it");

    check(fs::copy_file(dir / "a.txt", dir / "b.txt", ec) && !ec,
          "a file is copied");
    check(fs::file_size(dir / "b.txt", ec) == 10 && !ec,
          "and the copy has the same size");

    // ⭐⭐ AND THE TWO OPERATIONS openkal HAS NO ATOM FOR, CHECKED AS REFUSALS.
    //
    // `kal_node_info` carries a boolean `writable` and not a mode word, and
    // SURFACE.txt has no operation that creates a symbolic link. openkal-musl
    // therefore refuses `chmod` and `symlink` rather than succeeding and
    // reporting something else afterwards --- and a refusal that arrives as a
    // `std::error_code` is what a C++ caller can act upon.
    //
    // ⚠️ THIS IS THE HALF THAT WOULD BE OMITTED. A probe checking only that the
    // supported operations work would pass just as well for a port that
    // silently accepted these two, which is the outcome the report
    // (openkal-linux#13) described as "expected 0600, got 0777".
    ec.clear();
    fs::permissions(dir / "a.txt", fs::perms::owner_read, ec);
    check(static_cast<bool>(ec), "changing permission bits is refused, not ignored");

    ec.clear();
    fs::create_symlink(dir / "a.txt", dir / "link", ec);
    check(static_cast<bool>(ec), "creating a symbolic link is refused, not ignored");

    fs::remove_all(dir, ec);
    check(!fs::exists(dir, ec), "the directory and its contents are removed");

    // --- std::random_device, which is the C++ face of openkal.random --------

    {
        std::random_device rd;
        const unsigned a = rd(), b = rd(), c = rd();
        // ⚠️ THE CRITERION IS THAT THEY DIFFER, NOT THAT ANY ONE OF THEM IS
        // ANYTHING. A source stuck at a constant satisfies "a number was
        // produced" and is exactly what a port that forgot to fill the buffer
        // would produce.
        check(!(a == b && b == c), "three draws from the entropy source differ");
    }

    std::printf("-- failures: %d --\n", failures);
    return failures != 0;
}
