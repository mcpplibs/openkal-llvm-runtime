import std;

int main() {
    std::vector<int> v{4, 2, 7};
    std::ranges::sort(v);
    std::println("import std above openkal: {} {} {}", v[0], v[1], v[2]);
    return v[0] == 2 ? 0 : 1;
}
