#include "update/sdk/Outcome.h"
#include "update/sdk/Update.h"
#include <iostream>
using namespace daymark::update;
static unsigned checks = 0;
static void expect(bool condition) {
    ++checks;
    if (!condition)
        throw std::runtime_error("assertion failed");
}
template <class F> void rejects(F test) {
    bool failed = false;
    try {
        test();
    } catch (const Failure &) {
        failed = true;
    }
    expect(failed);
}
int main() {
    try {
        expect(std::get<int>(invoke([] { return 7; })) == 7);
        const auto failed = invoke([]() -> Done { fail("update.cancelled", 50); });
        expect(std::get<UpdateError>(failed).key == "update.cancelled");
        expect(std::get<UpdateError>(failed).exitCode == 50);
        expect(Version::parse("2.10.0") > Version::parse("2.9.9"));
        expect(Version::parse("0.0.0").text() == "0.0.0");
        for (auto text : {"1.0", "01.2.3", "1.2.3.4", "1.2.3-beta", "-1.0.0", "4294967296.0.0",
                          "1..2", "1.2.03"})
            rejects([&] { Version::parse(text); });
        rejects([] { parseJson("{\"a\":1,\"a\":2}"); });
        rejects([] { parseJson("{\"a\":{\"b\":1,\"b\":2}}"); });
        expect(parseJson("{\"a\":{\"b\":1},\"b\":{\"b\":2}}").size() == 2);
        for (auto path : {"../a", "a/../b", "C:/test", "/test", "a\\b", "a:x", "CON.txt", "a//b",
                          "a./b", "nul", "com1.dll", "a/"})
            expect(!safeRelative(path));
        expect(safeRelative("versions/2.1.0-ab12/plugins/qwindows.dll"));
        for (auto text : {"", "a", "ab", "abc", "longer value"}) {
            Bytes bytes(text, text + std::char_traits<char>::length(text));
            expect(decodeBase64(encodeBase64(bytes)) == bytes);
        }
        rejects([] { decodeBase64("AB=="); });
        rejects([] { decodeBase64("YQ=A"); });
        std::cout << "PASS: " << checks << " SDK boundary assertions\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
