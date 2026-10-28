// Host test for text_utils (no board needed):
//   g++ -std=c++17 -Wall -Wextra -Werror -I.. ../text_utils.cpp text_utils_test.cpp -o text_utils_test && ./text_utils_test
#include "text_utils.h"

#include <cstdio>
#include <string>
#include <vector>

static int failures = 0;

static void expectEq(const std::string &got, const std::string &want, const char *what)
{
    if (got != want) {
        std::printf("FAIL %s\n  got:  \"%s\"\n  want: \"%s\"\n", what, got.c_str(), want.c_str());
        ++failures;
    }
}

static void expectLines(const std::vector<std::string> &got, const std::vector<std::string> &want, const char *what)
{
    std::string g, w;
    for (const auto &s : got) {
        g += "[" + s + "]";
    }
    for (const auto &s : want) {
        w += "[" + s + "]";
    }
    expectEq(g, w, what);
}

int main()
{
    using namespace text_utils;

    expectEq(utf8ToAscii("Xin chào, đây là Claude!"), "Xin chao, day la Claude!", "basic vietnamese");
    expectEq(utf8ToAscii("ĂÂĐÊÔƠƯ ăâđêôơư"), "AADEOOU aadeoou", "base letters");
    expectEq(utf8ToAscii("Ấ ầ Ẩ ẫ Ặ ẹ Ế ỉ Ị ọ Ố ờ Ở ỡ Ợ ụ Ứ ừ Ử ữ Ự ỳ Ỵ ỷ Ỹ"),
             "A a A a A e E i I o O o O o O u U u U u U y Y y Y", "tone marks");
    expectEq(utf8ToAscii("Tiếng Việt: Hải Phòng, Lê Chân"), "Tieng Viet: Hai Phong, Le Chan", "place names");
    expectEq(utf8ToAscii("“Ngoặc” ‘đơn’ – gạch … 25°C"), "\"Ngoac\" 'don' - gach ... 25\xF7" "C", "punctuation");
    expectEq(utf8ToAscii("Chào 👋🏻 bạn ✨!"), "Chao  ban !", "emoji dropped");
    expectEq(utf8ToAscii("Tie\xcc\x82\xcc\x81ng"), "Tieng", "decomposed marks dropped");
    expectEq(utf8ToAscii("a\r\nb\tc"), "a\nb c", "control chars");
    expectEq(utf8ToAscii("bad \xff byte"), "bad ? byte", "invalid utf8");
    expectEq(utf8ToAscii("cut \xe1\xba"), "cut ?", "truncated utf8");
    expectEq(utf8ToAscii("中文"), "??", "unsupported script");

    expectEq(stripMarkdown("# Tieu de\n**Dam** va __gach__ `code`"), "Tieu de\nDam va gach code", "markdown");
    expectEq(stripMarkdown("2 * 3 = 6, a_b"), "2 * 3 = 6, a_b", "single markers kept");

    expectLines(wrapText("mot hai ba bon nam", 7), {"mot hai", "ba bon", "nam"}, "wrap words");
    expectLines(wrapText("abcdefghij", 4), {"abcd", "efgh", "ij"}, "split long word");
    expectLines(wrapText("dong 1\n\ndong 3", 20), {"dong 1", "", "dong 3"}, "blank lines kept");
    expectLines(wrapText("  cach   nhieu  ", 20), {"cach nhieu"}, "collapse spaces");
    expectLines(wrapText("", 10), {""}, "empty text");
    expectLines(wrapText("x\n", 10), {"x", ""}, "trailing newline");

    std::string reply = "  [Happy] Chao ban!";
    expectEq(takeMoodTag(reply), "happy", "mood tag");
    expectEq(reply, "Chao ban!", "mood tag removed");
    reply = "[surprised]\nOi!";
    expectEq(takeMoodTag(reply), "surprised", "mood tag before newline");
    expectEq(reply, "Oi!", "newline after tag removed");
    reply = "Khong co tag [happy]";
    expectEq(takeMoodTag(reply), "", "tag not at start");
    expectEq(reply, "Khong co tag [happy]", "text kept without tag");
    reply = "[1, 2] la mang";
    expectEq(takeMoodTag(reply), "", "brackets that are not a tag");
    expectEq(reply, "[1, 2] la mang", "text kept for non-tag");
    reply = "[]";
    expectEq(takeMoodTag(reply), "", "empty tag");

    if (failures == 0) {
        std::printf("text_utils: all tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
