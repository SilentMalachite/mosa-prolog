% case: atom
foo.

% case: quoted_space
% expect: 'a b'
'a b'.

% case: empty_atom
''.

% case: japanese_atom
% expect: 親(太郎,一郎)
親(太郎, 一郎).

% case: non_bmp_atom
% expect: 𠮷
𠮷.

% case: quoted_japanese
% expect: '東 京'
'東 京'.

% case: shared_var
f(X, X).

% case: distinct_anon
f(_, _).

% case: integer
42.

% case: unary_minus
% expect: -42
-42.

% case: radix_bin
0b1010.

% case: radix_oct
0o17.

% case: radix_hex
0xFF.

% case: radix_underscore
1_000.

% case: boxed_int
9223372036854775807.

% case: char_a
0'a.

% case: char_hiragana
0'あ.

% case: char_non_bmp
0'𠮷.

% case: float_simple
1.5.

% case: float_one
1.0.

% case: float_exponent
1.5e-2.

% case: list_empty
[].

% case: list_pair
[a, b].

% case: list_bar
[H|T].

% case: curly_one
{a}.

% case: curly_comma
{a, b}.

% case: compound
f(a, b).

% case: priority_times
a+b*c.

% case: priority_plus
(a+b)*c.

% case: assoc_left
a-b-c.

% case: assoc_right
a^b^c.

% case: comma_two
f(a, b).

% case: comma_one
f((a, b)).

% case: escape_n
'\n'.

% case: escape_x
'\x41\'.

% case: escape_u
'\u3042'.

% case: escape_cont
'ab\
c'.

% case: double_quotes
% expect: [97,98]
"ab".

% case: comment
/* skip */ foo.

% case: japanese_op
:- op(700, xfx, は).
太郎 は 学生.
