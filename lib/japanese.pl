:- module(japanese, [ひらがな_カタカナ/2, 全角_半角/2, 読み幅/2]).

% 幅の表は system だけが持つ。
読み幅(Text, Width) :- system:読み幅(Text, Width).

% 写像は system だけが持つ。
ひらがな_カタカナ(Hira, Kata) :- system:ひらがな_カタカナ(Hira, Kata).
全角_半角(Full, Half) :- system:全角_半角(Full, Half).
