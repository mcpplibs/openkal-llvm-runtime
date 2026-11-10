// A second name for a file through the C++ library: std::filesystem's
// create_hard_link and equivalent reach `link' and `stat' of openkal-musl
// 0.29.0, over kal_fs_hard_link (openkal 0.25). The count of a file's names is
// not asserted: openkal's node information carries no such count, and
// hard_link_count answers 1.
import std;
import mcpplibs.mtest;

using namespace mcpplibs::mtest;

UTest hard_link_test{"hard_link", [] {
    namespace fs = std::filesystem;
    const fs::path a = "okr-link-a.tmp", b = "okr-link-b.tmp";
    std::error_code ec;
    fs::remove(a, ec); fs::remove(b, ec);
    { std::ofstream(a) << "linked"; }
    ec.clear();
    fs::create_hard_link(a, b, ec);
    if (ec == std::errc::not_supported || ec == std::errc::operation_not_supported)
        skip("this volume keeps one name per file");
    expect(!ec) << "a file is given a second name (" << ec.message() << ")";
    expect(fs::equivalent(a, b, ec) && !ec) << "and the two names are one file";
    std::string text;
    { std::ifstream in(b); std::getline(in, text); }
    expect(eq(text, std::string("linked"))) << "the second name reads as the first";
    ec.clear();
    fs::create_hard_link(a, b, ec);
    expect(ec == std::errc::file_exists) << "a second name that is taken is refused";
    fs::remove(a, ec);
    text.clear();
    { std::ifstream in(b); std::getline(in, text); }
    expect(!fs::exists(a) && eq(text, std::string("linked")))
        << "removing one name leaves the file under the other";
    fs::remove(b, ec);
}};
