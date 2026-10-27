Built-in predicates, one file per ISO chapter. `length/2` has no clause in ISO/IEC 13211-1 and lives in `length.cpp`. Qualified database heads live in `database_home.cpp` so `database.cpp` stays within the file limit.

組込述語を置く。ISO の章ごとに 1 ファイルとする。`length/2` は ISO/IEC 13211-1 に条項が無いので `length.cpp` に置く。修飾したデータベースの頭部は `database_home.cpp` に置き、`database.cpp` を行数の上限に収める。
