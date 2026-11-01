// レビュー M5。書き込みで開いたファイルは、close も flush_output も無くても、halt とプロセスの終わりで中身が残る。
// mosa（run_session）、埋め込み（Machine）、halt の各経路で確かめる。閉じた流れと読み込みの流れは書き戻さない。

#include "harness.hpp"
#include "mosa/machine.hpp"
#include "mosa/path_text.hpp"
#include "mosa/repl.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

std::string tmp_path(std::string_view name) {
  return std::string(MOSA_SOURCE_DIR) + "/tests/tmp/" + std::string(name);
}

// 無いファイルと空のファイルを見分ける。
std::optional<std::string> slurp(const std::string& path) {
  std::ifstream input(mosa::path_from_utf8(path), std::ios::binary);
  if (!input) return std::nullopt;
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

void put_file(const std::string& path, std::string_view bytes) {
  std::ofstream output(mosa::path_from_utf8(path), std::ios::binary | std::ios::trunc);
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void fresh(const std::string& path) {
  std::error_code code;
  std::filesystem::remove(mosa::path_from_utf8(path), code);
}

bool holds(const std::string& path, std::string_view expected) {
  const auto got = slurp(path);
  return got && *got == expected;
}

std::string shown(const std::string& path) {
  const auto got = slurp(path);
  return got ? "[" + *got + "]" : "(missing)";
}

mosa::SessionResult batch(const std::vector<std::string>& words, std::size_t limit = mosa::kDefaultHeapMax) {
  std::vector<std::string_view> args;
  args.reserve(words.size() + 1);
  args.emplace_back("mosa");
  for (const auto& word : words) args.emplace_back(word);
  return mosa::run_session(args, "", false, limit);
}

// レビューの再現そのもの。前の中身は write で消えてよいが、新しい中身は残る。
void check_review_repro() {
  const auto path = tmp_path("m5_repro.txt");
  fresh(path);
  const auto goal = "open('" + path + "', write, W), write(W, kept), close(W), open('" + path +
                    "', write, S), write(S, lost).";
  const auto with_t = batch({"-g", goal, "-t", "halt"});
  mosa_test::check(with_t.code == 0, "repro -t halt exit");
  mosa_test::check(holds(path, "lost"), "repro -t halt keeps the new text, got " + shown(path));
  fresh(path);
  const auto inside = batch({"-g", goal.substr(0, goal.size() - 1) + ", halt."});
  mosa_test::check(inside.code == 0, "repro halt in -g exit");
  mosa_test::check(holds(path, "lost"), "repro halt in -g keeps the new text, got " + shown(path));
}

void check_halt_code_and_modes() {
  const auto path = tmp_path("m5_halt3.txt");
  fresh(path);
  const auto halted = batch({"-g", "open('" + path + "', write, S), write(S, three), nl(S), halt(3)."});
  mosa_test::check(halted.code == 3, "halt(3) exit " + std::to_string(halted.code));
  mosa_test::check(holds(path, "three\n"), "halt(3) keeps text, got " + shown(path));
  const auto app = tmp_path("m5_append.txt");
  put_file(app, "old,");
  const auto appended = batch({"-g", "open('" + app + "', append, S), write(S, new), halt."});
  mosa_test::check(appended.code == 0, "append exit");
  mosa_test::check(holds(app, "old,new"), "append keeps old and new, got " + shown(app));
  const auto bin = tmp_path("m5_binary.bin");
  fresh(bin);
  const auto binary = batch({"-g", "open('" + bin + "', write, S, [type(binary)]), put_byte(S, 0), "
                                   "put_byte(S, 255), put_byte(S, 10), halt."});
  mosa_test::check(binary.code == 0, "binary exit");
  mosa_test::check(holds(bin, std::string_view("\x00\xff\n", 3)), "binary bytes kept, got " + shown(bin));
}

// halt を通らない終わり方。ゴールの成功、失敗、例外、ヒープの上限。
void check_ends_without_halt() {
  const auto ok = tmp_path("m5_success.txt");
  fresh(ok);
  const auto succeeded = batch({"-g", "open('" + ok + "', write, S), write(S, done)."});
  mosa_test::check(succeeded.code == 0, "success exit");
  mosa_test::check(holds(ok, "done"), "goal success keeps text, got " + shown(ok));
  const auto no = tmp_path("m5_failure.txt");
  fresh(no);
  const auto failed = batch({"-g", "open('" + no + "', write, S), write(S, before_fail), fail."});
  mosa_test::check(failed.code == 1, "failure exit " + std::to_string(failed.code));
  mosa_test::check(holds(no, "before_fail"), "goal failure keeps text, got " + shown(no));
  const auto ex = tmp_path("m5_throw.txt");
  fresh(ex);
  const auto thrown = batch({"-g", "open('" + ex + "', write, S), write(S, before_throw), throw(oops)."});
  mosa_test::check(thrown.code != 0, "throw exit");
  mosa_test::check(holds(ex, "before_throw"), "uncaught exception keeps text, got " + shown(ex));
}

void check_heap_limit_stop() {
  const auto program = tmp_path("m5_grow.pl");
  put_file(program, "grow(L) :- grow([x|L]).\n");
  const auto out = tmp_path("m5_heap.txt");
  fresh(out);
  const auto stopped = batch({program, "--heap-max", "1", "-g", "open('" + out + "', write, S), write(S, before_limit), "
                                                                "grow([])."});
  mosa_test::check(stopped.code == 2, "heap limit exit " + std::to_string(stopped.code));
  mosa_test::check(holds(out, "before_limit"), "heap limit stop keeps text, got " + shown(out));
}

// 読み込みの指示と initialization(main, main) の中の書き込み。
void check_directive_and_main() {
  const auto out = tmp_path("m5_directive.txt");
  fresh(out);
  const auto program = tmp_path("m5_directive.pl");
  put_file(program, ":- open('" + out + "', write, S), write(S, directive), halt(5).\n:- throw(after).\n");
  const auto directive = batch({program});
  mosa_test::check(directive.code == 5, "directive halt exit " + std::to_string(directive.code));
  mosa_test::check(holds(out, "directive"), "directive halt keeps text, got " + shown(out));
  const auto main_out = tmp_path("m5_main.txt");
  fresh(main_out);
  const auto main_program = tmp_path("m5_main.pl");
  put_file(main_program, ":- initialization(main, main).\nmain :- open('" + main_out +
                             "', write, S), set_output(S), write(via_current), nl.\n");
  const auto mained = batch({main_program});
  mosa_test::check(mained.code == 0, "main exit " + std::to_string(mained.code));
  mosa_test::check(holds(main_out, "via_current\n"), "main via set_output keeps text, got " + shown(main_out));
  mosa_test::check(mained.out.empty(), "main output went to the file, not stdout: " + mained.out);
}

// 対話の halt。端末の入力を与える。
void check_repl_halt() {
  const auto out = tmp_path("m5_repl.txt");
  fresh(out);
  std::vector<std::string_view> args{"mosa"};
  const std::string input = "open('" + out + "', write, S, [alias(m5_repl)]).\nwrite(m5_repl, typed).\nhalt.\n";
  const auto talked = mosa::run_session(args, input, true);
  mosa_test::check(talked.code == 0, "repl exit");
  mosa_test::check(holds(out, "typed"), "repl halt keeps text, got " + shown(out));
}

// 位置を戻して上書きした結果。末尾を切ったり、追記にしたりしない。
void check_reposition() {
  const auto out = tmp_path("m5_reposition.txt");
  fresh(out);
  const auto moved = batch({"-g", "open('" + out + "', write, S, [reposition(true)]), stream_property(S, position(P)), "
                                  "write(S, abc), set_stream_position(S, P), write(S, 'X'), halt."});
  mosa_test::check(moved.code == 0, "reposition exit " + moved.err);
  mosa_test::check(holds(out, "Xbc"), "reposition overwrite kept, got " + shown(out));
}

void check_japanese_path() {
  const auto out = tmp_path("m5_日本語.txt");
  fresh(out);
  const auto wrote = batch({"-g", "open('" + out + "', write, S), write(S, '書き込み'), halt."});
  mosa_test::check(wrote.code == 0, "japanese exit");
  mosa_test::check(holds(out, "書き込み"), "japanese path and text kept, got " + shown(out));
}

// 入れ子の中の halt。with_output_to・findall・\+・catch・print の portray は、どれも中で halt すると外まで止まる。
void check_nested_halt() {
  const std::vector<std::pair<std::string, std::string>> wraps{
      {"with_output_to", "with_output_to(atom(_), (write(S, x), halt))"},
      {"findall", "findall(_, (write(S, x), halt), _)"},
      {"not", "\\+ (write(S, x), halt)"},
      {"catch", "catch((write(S, x), halt), _, true)"},
      {"catch_recovery", "catch((write(S, x), throw(b)), b, halt)"},
  };
  for (const auto& [label, body] : wraps) {
    const auto out = tmp_path("m5_nested_" + label + ".txt");
    fresh(out);
    const auto run = batch({"-g", "open('" + out + "', write, S), " + body + ", write(S, after)."});
    mosa_test::check(run.code == 0, "nested " + label + " exit " + std::to_string(run.code));
    mosa_test::check(holds(out, "x"), "nested " + label + " keeps text, got " + shown(out));
  }
  const auto out = tmp_path("m5_nested_portray.txt");
  fresh(out);
  const auto program = tmp_path("m5_portray.pl");
  // portray の中の現在出力は捕獲用のメモリなので、別名で直にファイルへ書く。捕獲の分は halt で捨てる（L14）。
  put_file(program, ":- dynamic(portray/1).\nportray(boom) :- write(m5_portray, p), halt(6).\n");
  const auto run = batch({program, "-g", "open('" + out + "', write, _, [alias(m5_portray)]), print(boom), "
                                         "write(m5_portray, after)."});
  mosa_test::check(run.code == 6, "nested portray exit " + std::to_string(run.code));
  mosa_test::check(holds(out, "p"), "nested portray keeps text, got " + shown(out));
}

// 埋め込みの halt は Machine が生きている間に書き出す。流れは開いたままで、後の書き込みも残る。
void check_embedded_halt() {
  const auto out = tmp_path("m5_embed_halt.txt");
  fresh(out);
  {
    auto machine = mosa::Machine::create();
    const auto stop = machine->query("open('" + out + "', write, S, [alias(m5_embed)]), write(m5_embed, a), halt.");
    mosa_test::check(stop.kind == mosa::StopKind::Halt, "embed halt stops");
    mosa_test::check(holds(out, "a"), "embed halt flushes before the machine ends, got " + shown(out));
    const auto again = machine->query("write(m5_embed, b), stream_property(S, alias(m5_embed)).");
    mosa_test::check(again.kind == mosa::StopKind::Success, "embed stream stays open after halt");
  }
  mosa_test::check(holds(out, "ab"), "embed writes after halt reach the file at the end, got " + shown(out));
  const auto api = tmp_path("m5_embed_api.txt");
  fresh(api);
  auto machine = mosa::Machine::create();
  const auto opened = machine->query("open('" + api + "', write, S), write(S, api).");
  mosa_test::check(opened.kind == mosa::StopKind::Success, "embed api open");
  machine->halt(0);
  mosa_test::check(holds(api, "api"), "Machine::halt flushes, got " + shown(api));
}

// 終わりに書き戻すのは、開いている書き込みの流れだけ。閉じた流れや読み込みの流れが、外で変わった中身を
// 古い写しで上書きしてはならない。
void check_embedded_leaves_others() {
  const auto closed = tmp_path("m5_closed.txt");
  const auto read = tmp_path("m5_read.txt");
  const auto open_one = tmp_path("m5_open.txt");
  put_file(read, "source");
  fresh(open_one);
  {
    auto machine = mosa::Machine::create();
    const auto stop = machine->query("open('" + closed + "', write, C), write(C, inside), close(C), open('" + read +
                                     "', read, R), get_char(R, _), open('" + open_one + "', write, W), write(W, w).");
    mosa_test::check(stop.kind == mosa::StopKind::Success, "embed leaves: query");
    put_file(closed, "outside");
    put_file(read, "changed");
    const auto halted = machine->query("halt.");
    mosa_test::check(halted.kind == mosa::StopKind::Halt, "embed leaves: halt");
    mosa_test::check(holds(closed, "outside") && holds(read, "changed"), "halt leaves closed and read streams");
  }
  mosa_test::check(holds(closed, "outside"), "a closed stream is not written back, got " + shown(closed));
  mosa_test::check(holds(read, "changed"), "a read stream is not written back, got " + shown(read));
  mosa_test::check(holds(open_one, "w"), "the open write stream is written, got " + shown(open_one));
}

// 書いてから保存していない流れだけを書き出す（敵対的レビューの A1〜A3）。開いただけの流れ、flush の後に書いていない
// 流れ、halt で書き出した後に書いていない流れが、後で正しく書かれたファイルや外で変わった中身を潰してはならない。
void check_only_unsaved_streams() {
  const auto idle = tmp_path("m5_idle.txt");
  fresh(idle);
  const auto retried = batch({"-g", "(catch(open('" + idle + "', write, _), _, fail) -> true ; true), open('" + idle +
                                        "', write, S), write(S, result), close(S).", "-t", "halt"});
  mosa_test::check(retried.code == 0, "idle exit");
  mosa_test::check(holds(idle, "result"), "an idle write stream does not empty a file closed later, got " + shown(idle));
  const auto flushed = tmp_path("m5_flushed.txt");
  fresh(flushed);
  {
    auto machine = mosa::Machine::create();
    const auto stop = machine->query("open('" + flushed + "', write, S), write(S, stale), flush_output(S).");
    mosa_test::check(stop.kind == mosa::StopKind::Success && holds(flushed, "stale"), "flush writes");
    put_file(flushed, "fresh");
    const auto halted = machine->query("halt.");
    mosa_test::check(halted.kind == mosa::StopKind::Halt, "flushed: halt");
    mosa_test::check(holds(flushed, "fresh"), "halt does not rewrite a flushed stream, got " + shown(flushed));
  }
  mosa_test::check(holds(flushed, "fresh"), "the end does not rewrite a flushed stream, got " + shown(flushed));
  const auto once = tmp_path("m5_once.txt");
  fresh(once);
  {
    auto machine = mosa::Machine::create();
    const auto stop = machine->query("open('" + once + "', write, S), write(S, halted), halt.");
    mosa_test::check(stop.kind == mosa::StopKind::Halt && holds(once, "halted"), "once: halt writes");
    put_file(once, "host");
  }
  mosa_test::check(holds(once, "host"), "the end does not write again what halt wrote, got " + shown(once));
}

}  // namespace

int main() {
  check_review_repro();
  check_halt_code_and_modes();
  check_ends_without_halt();
  check_heap_limit_stop();
  check_directive_and_main();
  check_repl_halt();
  check_reposition();
  check_japanese_path();
  check_nested_halt();
  check_embedded_halt();
  check_embedded_leaves_others();
  check_only_unsaved_streams();
  return mosa_test::failures();
}
