English | [日本語](../ja/11-japanese.md)

# 11. Writing in Japanese

In this chapter you write atoms and variables with Japanese names, count the characters of an atom, write Prolog
with full-width punctuation, and choose the language of Mosa's messages.

To type Japanese, turn on a Japanese input method on your computer, or copy the examples from this page. An input
method is the part of your system that turns the keys you press into Japanese characters.

## Steps

1. Move into the folder `prolog` with the `cd` line of chapter 4, and start `mosa`. Write atoms in Japanese. Kanji,
   hiragana and katakana have no capital letters, so Mosa reads a word in them as an atom, as it reads a word that
   starts with a lowercase letter. Such a word needs no quotes. `東京` is Tokyo, and the list holds an apple, a
   banana and sushi.

<!-- check: atoms -->
```text
?- X = 東京.
X = 東京.
?- X = [りんご, バナナ, 寿司].
X = [りんご,バナナ,寿司].
```

2. Give a variable a Japanese name. `名前` (name) is an atom, and so is `太郎` (Taro, a given name). So
   `名前 = 太郎` asks whether two different atoms are the same, and the answer is `false.`. A variable with a Japanese
   name starts with an underscore, as in `_名前`. Type `help.` to see this rule again: `help` is a command of the
   `?-` prompt, not a predicate, and Mosa answers it with one line and no `true.`.

<!-- check: variables -->
```text
?- 名前 = 太郎.
false.
?- _名前 = 太郎.
_名前 = 太郎.
?- help.
Variables with Japanese names start with an underscore, as in _太郎. Without it, 太郎 is an atom.
```

3. Count the characters of an atom. For Mosa, one character is one Unicode code point. Unicode is the standard that
   gives each character of every writing system a number, called its code point. So `東京` has 2 characters, and
   `𠮷野家` has 3, although some other programs count `𠮷`, a rare form of `吉`, as two. `atom_chars/2` gives the
   list of the characters of an atom. Copy `𠮷` from this page if your input method cannot type it.

<!-- check: count -->
```text
?- atom_length(東京, N).
N = 2.
?- atom_length('𠮷野家', N).
N = 3.
?- atom_chars(日本語, L).
L = [日,本,語].
```

4. See two atoms that look the same but are different. `atom_codes/2` gives the list of the code points of an atom,
   and with a list of code points it makes the atom. `が` is the code point 12364. The same picture can also be made
   from two code points: `か` (12363), followed by the combining voiced sound mark (12441), which your screen draws
   on top of the character before it. The second query makes that atom. It looks like `が`, but it has two
   characters, and the third query shows that it is not the atom `が` that you typed.

<!-- check: normalization -->
```text
?- atom_codes(が, L).
L = [12364].
?- atom_codes(A, [12363, 12441]), atom_length(A, N).
A = が, N = 2.
?- atom_codes(A, [12363, 12441]), A = が.
false.
```

5. Type a query with full-width punctuation. A Japanese input method types full-width characters, which are as wide
   as a kanji: `（` instead of `(`, `、` instead of `,`, `。` instead of `.`. Mosa has settings called flags, each
   with a name and a value. `current_prolog_flag(Name, Value)` gives the value of a flag. The flag
   `japanese_punctuation` is `false` when Mosa starts, so Mosa does not read `（` as a parenthesis, and names
   column 6, where it stopped. `親（子）` is "parent(child)".

<!-- check: punct-off -->
```text
?- current_prolog_flag(japanese_punctuation, F).
F = false.
?- X = 親（子）。
Syntax error: line 1 column 6: unexpected token （
```

6. Turn on full-width symbol mode with `set_prolog_flag/2`, which changes the value of a flag. Now Mosa reads the
   full-width punctuation as the half-width one. It always writes its answers in half-width. `「` and `」` are
   Japanese quotation marks, and they work as single quotes. Then turn the mode off again, for the next steps.

<!-- check: punct-on -->
```text
?- set_prolog_flag(japanese_punctuation, true).
true.
?- X = 親（子）。
X = 親(子).
?- X = 親（太郎、一郎）。
X = 親(太郎,一郎).
?- X = 「東 京」。
X = '東 京'.
?- set_prolog_flag(japanese_punctuation, false).
true.
```

7. Write a file with full-width punctuation. Create a file `capital.pl` in the folder `prolog` with these lines.
   The first line is a directive: a clause that starts with `:-`, with nothing before it. Mosa does not store a
   directive, but runs the goal after `:-` while it loads the file. This one turns on full-width symbol mode for the
   rest of the file. Write the directive itself in half-width, because the mode is still off when Mosa reads it.
   Each fact says that the capital of a country is a city: Japan and Tokyo, France and Paris, and the United States
   and Washington, D.C., whose name needs quotes because it holds a space and periods.

<!-- file: capital-file capital.pl -->
```prolog
:- set_prolog_flag(japanese_punctuation, true).

capital（日本、東京）。
capital（フランス、パリ）。
capital（アメリカ、「ワシントン D.C.」）。
```

8. Load the file, and ask for every capital in half-width, pressing `;` until the period. Then look at the flag: the
   directive turned the mode on only up to the end of the file, so at the prompt it is off again.

<!-- check: punct-file
consult('capital.pl').
capital(X, Y).
;
;
current_prolog_flag(japanese_punctuation, F).
-->
```text
?- consult('capital.pl').
true.
?- capital(X, Y).
X = 日本, Y = 東京;

X = フランス, Y = パリ;

X = アメリカ, Y = 'ワシントン D.C.'.
?- current_prolog_flag(japanese_punctuation, F).
F = false.
```

9. Change the language of Mosa's messages. The flag `message_language` is `en` (English) or `ja` (Japanese). Set it
   to `ja`, and make the syntax error of chapter 6 and the warning of chapter 3. The text around the error is now in
   Japanese, but the error term itself, `error(existence_error(…), …)`, stays the same. Set the flag back to `en`.

<!-- check: language -->
```text
?- current_prolog_flag(message_language, L).
L = en.
?- set_prolog_flag(message_language, ja).
true.
?- 3 <= 5.
構文エラー: 行 1 列 3: 予期しないトークン <=
?- hello.
警告: ゴールは例外を送出しました: error(existence_error(procedure,hello/0),context(hello/0,_G1))
?- set_prolog_flag(message_language, en).
true.
```

10. Type `halt.` to quit. Then start Mosa with its messages in Japanese from the beginning. On macOS and Linux, put
    `LANG=ja_JP.UTF-8` before `mosa`, which sets it for this one command:

<!-- skip: lang-unix -->
```console
$ LANG=ja_JP.UTF-8 mosa
```

In PowerShell on Windows, set `LANG` for this window first, and then start `mosa`:

<!-- skip: lang-windows -->
```powershell
$env:LANG = "ja_JP.UTF-8"
mosa
```

Type `hello.`, and the warning is in Japanese, as in step 9. Type `halt.` to quit. In PowerShell, `LANG` stays set
until you close the window.

## What happened

Mosa reads every file and every line that you type as UTF-8, a common way of storing Unicode text. It sorts each
character into a class by its Unicode category:

- Letters without case, such as kanji, hiragana and katakana, count as lowercase letters. A word that starts with
  one is an atom.
- Only capital letters (`A` to `Z`, and capitals of other alphabets such as `Ä` or `Σ`) and the underscore `_` start
  a variable. So a variable with a Japanese name starts with `_`, or with a capital letter, as in `X人`.
- A full-width space counts as a space.
- Digits are only the half-width `0` to `9`. A full-width `１` is an illegal character, also in full-width symbol
  mode, so Mosa answers a query that holds one with a syntax error.

Mosa counts characters as Unicode code points in every built-in predicate, so `atom_length/2`, `atom_chars/2` and
`atom_codes/2` agree with each other.

Mosa compares two atoms by their bytes in UTF-8. It does not apply Unicode normalization (NFC or NFD), which some
other programs use to turn the two forms of `が` into one. So two atoms that look the same on the screen can be
different atoms, as in step 4. Text that you type with an input method usually has the one-code-point form. Text
that you copy from elsewhere may hold the two-code-point form: if a fact copied from another program does not match
the same word that you typed, compare the two with `atom_length/2` or `atom_codes/2`.

In full-width symbol mode, Mosa reads these full-width characters as the half-width ones:

| Full-width | Read as |
| --- | --- |
| `（` `）` | `(` `)` |
| `［` `］` | `[` `]` |
| `，` `、` | `,` |
| `。` | `.` at the end of a clause or a query, never as a decimal point |
| `：` | `:` |
| `「` `」` | `'` at the start and at the end of a quoted atom; the first `」` ends it, so they cannot be nested |

Other full-width characters are not changed: the full-width period `．` stays an error, and `3.14` must be written
with `.`. Set at the prompt, the flag lasts until you change it or quit. Set by a directive in a file, it lasts up to
the end of that file. A file that uses full-width punctuation without the directive does not load. To see the
message, save this one line as `nodirective.pl` in the folder `prolog`, and start `mosa` with it from your shell.
Mosa stops at the `（` in column 8 of line 1, as it stopped at the mistake of `typo.pl` in chapter 4, and returns to
your shell:

<!-- file: nodirective-file nodirective.pl -->
```prolog
capital（日本、東京）。
```

<!-- check: nodirective -->
```console
$ mosa nodirective.pl
nodirective.pl:1:8: unexpected token
```

Mosa chooses the language of its messages when it starts. When the environment variable `LANG`, a setting that your
shell passes to every program, starts with `ja`, the messages are in Japanese. When it has any other value, they are
in English. When `LANG` is not set, Mosa on Windows follows the Windows display language (Settings, then Time &
language), and Mosa on macOS and Linux uses English. The terminal on macOS and on Linux usually sets `LANG` from the
language of your system; type `echo $LANG` to see it:

<!-- skip: echo-lang -->
```console
$ echo $LANG
```

## Exercises

1. Ask Mosa for the characters of `ありがとう` (thank you), as a list, and for their number.
2. Load `capital.pl`, turn on full-width symbol mode at the prompt, and ask for the capital of France with
   full-width punctuation.

### Answers

1. `atom_chars/2` gives the list, and `length/2` counts it (chapter 7):

<!-- check: exercise-chars -->
```text
?- atom_chars(ありがとう, L), length(L, N).
L = [あ,り,が,と,う], N = 5.
```

2. The directive of `capital.pl` changed the mode only inside the file, so turn it on yourself:

<!-- check: exercise-capital -->
```text
?- consult('capital.pl').
true.
?- set_prolog_flag(japanese_punctuation, true).
true.
?- capital（フランス、X）。
X = パリ.
```

---

[Index](README.md) · Previous: [10. Input and output](10-input-output.md) · Next: 12. Modules and libraries (not
written yet)
