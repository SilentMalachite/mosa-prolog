#include "mosa/roundtrip.hpp"

#include "mosa/reader.hpp"
#include "mosa/writer.hpp"

#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

struct Example {
  std::string id;
  std::string expect;
  bool has_expect = false;
  std::string body;
};

const char* reason_name(SyntaxReason reason) {
  switch (reason) {
    case SyntaxReason::InvalidUtf8: return "InvalidUtf8";
    case SyntaxReason::IllegalCharacter: return "IllegalCharacter";
    case SyntaxReason::UnterminatedQuote: return "UnterminatedQuote";
    case SyntaxReason::UnterminatedComment: return "UnterminatedComment";
    case SyntaxReason::BadEscape: return "BadEscape";
    case SyntaxReason::IntegerOverflow: return "IntegerOverflow";
    case SyntaxReason::UnexpectedToken: return "UnexpectedToken";
    case SyntaxReason::MaxArity: return "MaxArity";
    case SyntaxReason::IncompleteTerm: return "IncompleteTerm";
  }
  return "error";
}

int fail(const Example& example, std::string_view detail) {
  std::cerr << "mosatest: " << example.id << ": " << detail << '\n';
  return 1;
}

std::vector<Example> split_examples(std::string_view text) {
  std::vector<Example> examples;
  Example current;
  bool open = false;
  std::size_t index = 0;
  while (index < text.size()) {
    std::size_t end = text.find('\n', index);
    const bool last = end == std::string_view::npos;
    if (last) end = text.size();
    std::string_view line = text.substr(index, end - index);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (line.starts_with("% case: ")) {
      if (open) examples.push_back(std::move(current));
      current = Example{};
      current.id = std::string{line.substr(8)};
      open = true;
    } else if (open && line.starts_with("% expect: ")) {
      current.expect = std::string{line.substr(10)};
      current.has_expect = true;
    } else if (open) {
      current.body.append(line);
      current.body.push_back('\n');
    }
    if (last) break;
    index = end + 1;
  }
  if (open) examples.push_back(std::move(current));
  return examples;
}

bool functor_named(const Heap& heap, Cell cell, std::string_view name, int arity) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Str && heap.functor_arity(cell) == static_cast<unsigned>(arity) &&
         heap.atoms().name(heap.functor_atom(cell)) == name;
}

std::optional<Specifier> specifier_of(std::string_view name) {
  if (name == "xf") return Specifier::Xf;
  if (name == "yf") return Specifier::Yf;
  if (name == "xfx") return Specifier::Xfx;
  if (name == "xfy") return Specifier::Xfy;
  if (name == "yfx") return Specifier::Yfx;
  if (name == "fx") return Specifier::Fx;
  if (name == "fy") return Specifier::Fy;
  return std::nullopt;
}

int apply_directive(const Example& example, Heap& heap, OperatorTable& operators, Cell term) {
  const Cell directive = heap.deref(heap.arg(term, 0));
  if (!functor_named(heap, directive, "op", 3)) return fail(example, "not op/3");
  const Cell priority = heap.deref(heap.arg(directive, 0));
  const Cell spec = heap.deref(heap.arg(directive, 1));
  const Cell name = heap.deref(heap.arg(directive, 2));
  if (priority.tag() != Tag::Int && priority.tag() != Tag::IntBox) return fail(example, "op priority");
  if (spec.tag() != Tag::Atom || name.tag() != Tag::Atom) return fail(example, "op args");
  const auto parsed = specifier_of(heap.atoms().name(static_cast<std::uint32_t>(spec.raw() >> 3)));
  if (!parsed) return fail(example, "op specifier");
  const auto atom = heap.atoms().name(static_cast<std::uint32_t>(name.raw() >> 3));
  if (operators.define(static_cast<int>(heap.integer_value(priority)), *parsed, std::string{atom}) !=
      OpResult::Ok) {
    return fail(example, "op rejected");
  }
  return 0;
}

int read_example(const Example& example, Heap& heap, OperatorTable& operators, ReadTerm& term) {
  Reader reader(example.body, heap, operators);
  bool saw_term = false;
  while (true) {
    const ReadOutcome outcome = reader.next();
    if (outcome.failure) {
      return fail(example, reason_name(outcome.failure->reason));
    }
    if (outcome.eof) break;
    const Cell cell = heap.deref(outcome.term->term);
    if (functor_named(heap, cell, ":-", 1)) {
      if (saw_term || apply_directive(example, heap, operators, cell) != 0) return 1;
      continue;
    }
    if (saw_term) return fail(example, "extra term");
    term = *outcome.term;
    saw_term = true;
  }
  if (!saw_term) return fail(example, "missing term");
  return 0;
}

int reread(const Example& example, const Heap& heap, const ReadTerm& term, const OperatorTable& operators,
           const std::string& text) {
  if (example.has_expect && text != example.expect) {
    return fail(example, "expect <" + example.expect + "> got <" + text + ">");
  }
  AtomTable atoms;
  Heap again(atoms);
  OperatorTable copy = operators;
  Reader reader(text + ".", again, copy);
  const ReadOutcome outcome = reader.next();
  if (!outcome.term) return fail(example, "reread");
  const ReadOutcome tail = reader.next();
  if (!tail.eof || tail.failure) return fail(example, "reread trailing");
  if (!same_term(heap, term.term, term.names, again, outcome.term->term, outcome.term->names)) {
    return fail(example, "mismatch <" + text + ">");
  }
  return 0;
}

int check_example(const Example& example) {
  AtomTable atoms;
  Heap heap(atoms);
  OperatorTable operators = OperatorTable::iso_core();
  ReadTerm term;
  if (const int code = read_example(example, heap, operators, term)) return code;
  const std::string text = write_term(heap, operators, term.term, WriteOptions{}, term.names);
  return reread(example, heap, term, operators, text);
}

}  // namespace

int run_roundtrip(std::string_view text) {
  const std::vector<Example> examples = split_examples(text);
  std::vector<std::string> seen;
  for (const Example& example : examples) {
    for (const std::string& id : seen) {
      if (id == example.id) {
        std::cerr << "mosatest: " << example.id << ": duplicate\n";
        return 1;
      }
    }
    seen.push_back(example.id);
    if (const int code = check_example(example)) return code;
  }
  return 0;
}

}
