Phase: 4 done (exit 2026-10-28: version mosa 0.0.1 (phase 4), CI green on 3 OSes in run 40683841050, suite 2041/2171 >= 2028)
In progress: none
Done (phase 4, modules): ADR 0006, predicates keyed by (module, name, arity); M:G; module/2, export/1, use_module/1,2 (as, except, bare file names); library(N) via $MOSA_LIB then lib/ next to the executable; database predicates take M:; current_module/1, predicate_property/2; meta_predicate/1 (src/module/meta.cpp); operators per module (src/module/module_ops.cpp); per-file double_quotes and japanese_punctuation
Done (phase 4, libraries): lists, apply, between, format ('$format_float'/4), strings (atomic_list_concat/2,3, split_atom/4, upcase_atom/2 over '$upcase_atom'/2 and k_upper_pairs), japanese (ひらがな_カタカナ/2, 全角_半角/2 both ways, 読み幅/2 over '$char_width'/2). Nothing is auto-imported into user
Done (phase 4, exit): tests/pl/modules/家系.pl holds every declaration of the SPEC section 5 example; modules.pl spec_example checks 家系:祖父(太郎, Z)
Done (phase 4, bugs): 6.3.4.1 negative literals in the reader; 7.9.1 is/2 checks the principal functor first (evaluable table in src/builtins/evaluable.hpp, checked by tests/cpp/evaluable_test.cpp); log/2 added
Done (after exit, Grok review): assert/retract/retractall/clause through a use_module `as` alias now act on the source predicate (heads rebuilt with named_head in src/module/target.cpp; retract redo too); predicate_property meta_predicate head uses the alias name. Rejected findings: ~*n count, nth0, POSIX MOSA_LIB path, partial import (not reproduced), forall(fail, 1) (forall_2 suite only checks Action under true)
Note: suite dirs between_3, succ_2, plus_3, format_2, atomic_list_concat_2/3 are still in the side column (not run); lib tests cover them
Note: lib error choices are in iso-notes Section 5 (succ/2, format/2, atomic_list_concat with a number result and an empty separator)
Next: phase 5 Task 1, take the baseline and build the triage table (phase 5 plan)
Open: interpretations left in iso-notes
Plan (phase 4, done): docs/superpowers/plans/2026-10-28-phase4-modules.md
Plan (phase 5): docs/superpowers/plans/2026-10-28-phase5-conformance-japanese.md
