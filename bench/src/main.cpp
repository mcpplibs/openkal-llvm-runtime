// What a compiler front end does with the C++ runtime, timed (0.17.0; the layer
// the design calls L2).
//
// ONE SOURCE, BUILT THREE WAYS: with the build machine's own compiler and C++
// library (`c++ -O2 -std=c++20 bench/src/main.cpp'), above this runtime on the
// machine's openkal implementation (`mcpp build --release' here), and as an ok
// image run by okrun. tools/bench-compare.sh divides two reports line by line.
//
// THE FIVE WORKLOADS are the shapes clangd's warm start spends its time in
// (mcpp's design of 2026-11-02, part 3, §2.3), written with the standard
// library alone, because this repository carries LLVM's runtimes and not its
// support libraries:
//
//   options    an option table searched by prefix for every argument of 481
//              command lines, as LLVM's OptTable parses a compile command
//   stringmap  a map from path strings to numbers filled, searched and emptied,
//              as StringMap is
//   densemap   an open-addressing map from pointers to numbers, as DenseMap is
//   bitstream  variable-width fields written and read back, as a module file's
//              bitstream is
//   index      posting lists of three-letter keys built and intersected, as a
//              symbol index is
//
// A line is `bench <name> <value> ns', the median of five runs after one
// unmeasured run; the value is per command line, per element, per field or per
// symbol (a run of the last four is whole passes over the data, divided).
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

double g_run_ns = 40e6;
volatile std::uint64_t g_sink;

std::uint64_t now_ns()
{
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

// Nanoseconds per unit of work of `body(n)', which does n units; the median of
// five runs after one unmeasured run.
template <class Body>
double measure(Body&& body)
{
    body(1);
    long n = 1;
    for (;;) {
        const auto t0 = now_ns();
        body(n);
        const auto dt = now_ns() - t0;
        if (dt > g_run_ns / 4 || n > (1L << 30)) { n = static_cast<long>(n * g_run_ns / (dt ? dt : 1)) + 1; break; }
        n *= 4;
    }
    double r[5];
    for (double& x : r) {
        const auto t0 = now_ns();
        body(n);
        x = static_cast<double>(now_ns() - t0) / static_cast<double>(n);
    }
    std::sort(r, r + 5);
    return r[2];
}

void report(const char* name, double ns)
{
    std::printf("bench %s %.2f ns\n", name, ns);
    std::fflush(stdout);
}

std::uint32_t g_seed = 12345;
std::uint32_t next()
{
    g_seed = g_seed * 1103515245u + 12345u;
    return g_seed >> 8;
}

// --- options ------------------------------------------------------------------

const char* const kStems[] = {
    "include", "isystem", "std", "W", "f", "m", "O", "g", "D", "U", "I", "o", "x", "c",
    "fmodule", "fprebuilt-module-path", "fmodules", "fno-exceptions", "fcolor-diagnostics",
    "march", "mtune", "target", "sysroot", "resource-dir", "fsyntax-only", "Xclang",
};
const char* const kWords[] = {
    "unused", "variable", "parameter", "function", "result", "value", "shadow", "sign",
    "conversion", "pedantic", "error", "extra", "all", "module", "file", "cache", "path",
    "output", "input", "inline", "vector", "align", "stack", "protector", "visibility",
};

struct options {
    std::vector<std::string> table;   // sorted
    std::vector<std::vector<std::string>> commands;

    options()
    {
        for (const char* s : kStems)
            for (const char* w : kWords) {
                table.push_back(std::string("-") + s + "-" + w);
                table.push_back(std::string("--") + s + "=" + w);
                table.push_back(std::string("-") + s + w + "=");
            }
        std::sort(table.begin(), table.end());
        table.erase(std::unique(table.begin(), table.end()), table.end());
        for (int c = 0; c < 481; ++c) {
            std::vector<std::string> args;
            const int n = 30 + static_cast<int>(next() % 40);
            for (int i = 0; i < n; ++i) {
                std::string a = table[next() % table.size()];
                if (a.back() == '=') a += "/home/user/project/src/module" + std::to_string(next() % 500) + ".cppm";
                args.push_back(std::move(a));
            }
            commands.push_back(std::move(args));
        }
    }

    // The longest option of the table that is a prefix of `arg', by the binary
    // search OptTable makes and the comparisons that follow it.
    std::size_t parse(const std::vector<std::string>& cmd) const
    {
        std::size_t found = 0;
        for (const std::string& arg : cmd) {
            const std::string_view a(arg);
            auto it = std::upper_bound(table.begin(), table.end(), a,
                                       [](std::string_view x, const std::string& y) { return x < std::string_view(y); });
            while (it != table.begin()) {
                --it;
                const std::string_view o(*it);
                if (a.substr(0, o.size()) == o) { found += o.size(); break; }
                if (o.empty() || o[0] != a[0] || (o.size() > 1 && a.size() > 1 && o[1] != a[1])) break;
            }
        }
        return found;
    }
};

// --- densemap -------------------------------------------------------------------

struct densemap {
    std::vector<std::pair<std::uintptr_t, std::uint32_t>> slots;
    std::size_t live = 0;
    static constexpr std::uintptr_t kEmpty = 0, kTomb = 1;

    explicit densemap(std::size_t cap = 64) : slots(cap, {kEmpty, 0}) {}
    std::size_t at(std::uintptr_t k) const { return ((k >> 4) ^ (k >> 9)) & (slots.size() - 1); }
    void grow()
    {
        densemap n(slots.size() * 2);
        for (auto& [k, v] : slots) if (k > kTomb) n.insert(k, v);
        *this = std::move(n);
    }
    void insert(std::uintptr_t k, std::uint32_t v)
    {
        if ((live + 1) * 4 >= slots.size() * 3) grow();
        for (std::size_t i = at(k);; i = (i + 1) & (slots.size() - 1))
            if (slots[i].first <= kTomb || slots[i].first == k) {
                if (slots[i].first <= kTomb) ++live;
                slots[i] = {k, v};
                return;
            }
    }
    const std::uint32_t* find(std::uintptr_t k) const
    {
        for (std::size_t i = at(k);; i = (i + 1) & (slots.size() - 1)) {
            if (slots[i].first == k) return &slots[i].second;
            if (slots[i].first == kEmpty) return nullptr;
        }
    }
    void erase(std::uintptr_t k)
    {
        for (std::size_t i = at(k);; i = (i + 1) & (slots.size() - 1)) {
            if (slots[i].first == k) { slots[i].first = kTomb; --live; return; }
            if (slots[i].first == kEmpty) return;
        }
    }
};

// --- bitstream ------------------------------------------------------------------

struct writer {
    std::vector<std::uint32_t> words;
    std::uint64_t cur = 0;
    unsigned bits = 0;
    void emit(std::uint32_t v, unsigned n)
    {
        cur |= static_cast<std::uint64_t>(v) << bits;
        bits += n;
        if (bits >= 32) { words.push_back(static_cast<std::uint32_t>(cur)); cur >>= 32; bits -= 32; }
    }
    void vbr(std::uint32_t v, unsigned n)
    {
        const std::uint32_t hi = 1u << (n - 1);
        while (v >= hi) { emit((v & (hi - 1)) | hi, n); v >>= n - 1; }
        emit(v, n);
    }
    void flush() { if (bits) { words.push_back(static_cast<std::uint32_t>(cur)); cur = 0; bits = 0; } }
};

struct reader {
    const std::vector<std::uint32_t>& words;
    std::size_t pos = 0;
    std::uint64_t cur = 0;
    unsigned bits = 0;
    std::uint32_t read(unsigned n)
    {
        if (bits < n) { cur |= static_cast<std::uint64_t>(pos < words.size() ? words[pos++] : 0) << bits; bits += 32; }
        const std::uint32_t v = static_cast<std::uint32_t>(cur & ((1ull << n) - 1));
        cur >>= n; bits -= n;
        return v;
    }
    std::uint32_t vbr(unsigned n)
    {
        const std::uint32_t hi = 1u << (n - 1);
        std::uint32_t v = 0; unsigned shift = 0;
        for (;;) {
            const std::uint32_t piece = read(n);
            v |= (piece & (hi - 1)) << shift;
            if (!(piece & hi)) return v;
            shift += n - 1;
        }
    }
};

}  // namespace

int main(int argc, char** argv)
{
    if (argc > 1 && std::strcmp(argv[1], "--quick") == 0) g_run_ns = 2e6;

    {
        const options o;
        report("options/command", measure([&](long n) {
            std::size_t s = 0;
            for (long i = 0; i < n; ++i) s += o.parse(o.commands[static_cast<std::size_t>(i) % o.commands.size()]);
            g_sink = g_sink + s;
        }));
    }

    {
        std::vector<std::string> keys;
        for (int i = 0; i < 20000; ++i)
            keys.push_back("/usr/include/c++/v1/__" + std::to_string(next() % 97) + "/component"
                           + std::to_string(i) + (i % 3 ? ".h" : ".cppm"));
        report("stringmap/element", measure([&](long n) {
            for (long r = 0; r < n; ++r) {
                std::unordered_map<std::string, int> m;
                for (std::size_t i = 0; i < keys.size(); ++i) m.emplace(keys[i], static_cast<int>(i));
                std::uint64_t s = 0;
                for (std::size_t i = 0; i < keys.size(); i += 2) s += static_cast<std::uint64_t>(m.find(keys[i])->second);
                for (std::size_t i = 0; i < keys.size(); i += 3) m.erase(keys[i]);
                g_sink = g_sink + s + m.size();
            }
        }) / static_cast<double>(keys.size()));
    }

    {
        std::vector<std::uintptr_t> ptrs;
        for (int i = 0; i < 200000; ++i) ptrs.push_back((static_cast<std::uintptr_t>(next()) << 6) | 64);
        report("densemap/element", measure([&](long n) {
            for (long r = 0; r < n; ++r) {
                densemap m;
                for (std::size_t i = 0; i < ptrs.size(); ++i) m.insert(ptrs[i], static_cast<std::uint32_t>(i));
                std::uint64_t s = 0;
                for (std::size_t i = 0; i < ptrs.size(); i += 2) if (auto* v = m.find(ptrs[i])) s += *v;
                for (std::size_t i = 0; i < ptrs.size(); i += 3) m.erase(ptrs[i]);
                g_sink = g_sink + s + m.live;
            }
        }) / static_cast<double>(ptrs.size()));
    }

    {
        std::vector<std::uint32_t> values;
        for (int i = 0; i < 200000; ++i) values.push_back(next() >> (next() % 24));
        report("bitstream/field", measure([&](long n) {
            for (long r = 0; r < n; ++r) {
                writer w;
                for (std::uint32_t v : values) w.vbr(v, 6);
                w.flush();
                reader rd{w.words};
                std::uint64_t s = 0;
                for (std::size_t i = 0; i < values.size(); ++i) s += rd.vbr(6);
                g_sink = g_sink + s;
            }
        }) / static_cast<double>(values.size()));
    }

    {
        std::vector<std::string> symbols;
        for (int i = 0; i < 30000; ++i)
            symbols.push_back(std::string(kWords[next() % 25]) + kStems[next() % 26] + std::to_string(i % 1000));
        report("index/symbol", measure([&](long n) {
            for (long r = 0; r < n; ++r) {
                std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> posting;
                for (std::uint32_t id = 0; id < symbols.size(); ++id) {
                    const std::string& s = symbols[id];
                    for (std::size_t i = 0; i + 3 <= s.size(); ++i) {
                        const std::uint32_t t = (static_cast<std::uint32_t>(static_cast<unsigned char>(s[i])) << 16)
                                              | (static_cast<std::uint32_t>(static_cast<unsigned char>(s[i + 1])) << 8)
                                              | static_cast<unsigned char>(s[i + 2]);
                        auto& list = posting[t];
                        if (list.empty() || list.back() != id) list.push_back(id);
                    }
                }
                std::uint64_t s = 0;
                for (int q = 0; q < 200; ++q) {
                    const auto& a = posting.begin()->second;
                    const auto& b = std::next(posting.begin(), q % static_cast<int>(posting.size()))->second;
                    std::vector<std::uint32_t> out;
                    std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(out));
                    s += out.size();
                }
                g_sink = g_sink + s + posting.size();
            }
        }) / static_cast<double>(symbols.size()));
    }

    std::printf("-- done --\n");
    return 0;
}
