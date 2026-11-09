// A program named without a separator is looked for by the conventions of the
// system the program runs on (openkal-musl 0.29.0; openkal clause 11 entry 30,
// `kal_env_system'). On Windows the PATH is split on `;', a name that carries
// a suffix is tried as given and one without it only with each PATHEXT suffix,
// so `hostname' is found as `hostname.exe' in System32; elsewhere the colon of
// POSIX is unchanged.
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <openkal/env.h>

import std;
import mcpplibs.mtest;

using namespace mcpplibs::mtest;

extern "C" char** environ;

namespace {

bool windows_conventions() {
    char name[16] = {};
    const kal_intptr n = kal_env_system(name, sizeof name);
    return n == 7 && std::memcmp(name, "windows", 7) == 0;
}

// Starts `file' through posix_spawnp with PATH (and PATHEXT, where given) set
// as stated, and answers its exit status, or -1 when it was not started.
int start(const char* file, const std::string& path, const char* pathext) {
    const char* old_path = std::getenv("PATH");
    const std::string kept_path = old_path ? old_path : "";
    const char* old_ext = std::getenv("PATHEXT");
    const std::string kept_ext = old_ext ? old_ext : "";
    setenv("PATH", path.c_str(), 1);
    if (pathext) setenv("PATHEXT", pathext, 1);
    char* argv[] = { const_cast<char*>(file), nullptr };
    pid_t pid = -1;
    const int e = posix_spawnp(&pid, file, nullptr, nullptr, argv, environ);
    if (old_path) setenv("PATH", kept_path.c_str(), 1); else unsetenv("PATH");
    if (pathext) { if (old_ext) setenv("PATHEXT", kept_ext.c_str(), 1); else unsetenv("PATHEXT"); }
    if (e != 0) return -1;
    int status = -1;
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status)) return -1;
    return WEXITSTATUS(status);
}

}  // namespace

UTest program_search_test{"program_search", [] {
    if (windows_conventions()) {
        const char* root = std::getenv("SystemRoot");
        if (!root) root = std::getenv("SYSTEMROOT");
        if (!root) skip("the system names no SystemRoot");
        const std::string system32 = std::string(root) + "\\System32";
        // An entry that does not exist first, so that the search has to go on.
        const std::string path = std::string("C:\\okr-absent-dir;") + system32;
        expect(eq(start("hostname", path, ".COM;.EXE;.BAT;.CMD"), 0))
            << "`hostname' is found as hostname.exe through a semicolon-separated PATH and PATHEXT";
        expect(eq(start("hostname", path, nullptr), 0))
            << "and through the default suffixes where PATHEXT is not set";
        expect(eq(start("hostname.exe", path, ".COM;.EXE"), 0))
            << "a name that carries its suffix is found as given";
        expect(eq(start("okr-no-such-program", path, ".EXE"), -1))
            << "a name found nowhere is not started";
        // A name without a suffix is tried only with the suffixes, as Windows'
        // command interpreter searches: a file `hostname' that is not a
        // program, in an earlier entry, is passed over for System32's
        // hostname.exe rather than ending the search.
        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path first = fs::current_path(ec) / "okr-first-entry";
        fs::create_directories(first, ec);
        { std::ofstream(first / "hostname") << "not a program\n"; }
        const std::string shadowed = first.string() + ";" + system32;
        expect(eq(start("hostname", shadowed, ".COM;.EXE"), 0))
            << "a suffixless file in an earlier entry does not stop the search for hostname.exe";
        fs::remove_all(first, ec);
    } else {
        if (access("/usr/bin/true", X_OK) != 0 && access("/bin/true", X_OK) != 0)
            skip("this system has no `true' to start");
        expect(eq(start("true", "/okr-absent-dir:/usr/bin:/bin", nullptr), 0))
            << "`true' is found past an entry that does not exist, through a colon-separated PATH";
        expect(eq(start("okr-no-such-program", "/okr-absent-dir:/usr/bin:/bin", nullptr), -1))
            << "a name found nowhere is not started";
    }
}};
