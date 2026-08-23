// `import std;` on a machine with no operating system.
//
// ⭐⭐ ONE SOURCE, TWO MACHINES, NOTHING EDITED BETWEEN THEM.
//
//     mcpp run                             — this machine, over openkal-linux
//     mcpp run --target riscv64-none-elf   — riscv64, over OpenSBI, no OS
//
// Both print the same four lines. That is the claim the design document makes
// about building above openkal, written as something a reader can run rather
// than something they have to believe.
//
// ⭐ AND THE SECOND COMMAND IS THE ACCEPTANCE CRITERION, BECAUSE IT CANNOT GO
// GREEN BY ACCIDENT.
//
// A hosted target has a C library, a C++ runtime and an unwinder already
// installed, and a program that reaches one of them by mistake still works ---
// so a build that was supposed to exercise this package's own copies can pass
// without having done so. There is nothing here to reach. The C library is
// openkal-musl, the standard library and the unwinder are this package's, and
// beneath them is firmware whose whole interface is `ecall`.
//
// Three assertions, chosen because each needs a different part of that stack:
//
//   the container and the algorithm need the allocator, which is openkal.memory
//   over a static region;
//
//   the formatting and the output need <format>'s machinery and a stream, which
//   is openkal.stream over the firmware's debug console;
//
//   the throw and the catch need libunwind reading .eh_frame and libc++abi
//   carrying the object --- and a destructor runs during the unwind, which is
//   what distinguishes an unwinder that WORKS from one that merely links.
import std;

namespace {

struct marker {
    bool* flag;
    ~marker() { *flag = true; }
};

bool unwound = false;

void thrower() {
    marker m{&unwound};
    throw 42;
}

}  // namespace

int main() {
    std::vector<int> v{4, 2, 7};
    std::ranges::sort(v);

    int caught = 0;
    try { thrower(); } catch (int e) { caught = e; }

    std::println("sorted: {} {} {}", v[0], v[1], v[2]);
    std::println("caught: {}", caught);
    std::println("unwound: {}", unwound);

    const bool ok = v[0] == 2 && v[2] == 7 && caught == 42 && unwound;
    // ⚠️ The outcome is an ARGUMENT and not the format string: a format string
    // is consumed by a consteval constructor, and a value computed at run time
    // cannot be one. The compiler says so, which is the whole reason it is
    // written this way.
    std::println("import std over openkal: {}", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
