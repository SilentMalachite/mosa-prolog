# Check in one edition, skip in the other

A fixture for `mosatest --manual` (`manual_fixture_bad_ids`). The Japanese edition skips the example this edition
checks, so the edition check fails even though the ids are the same.

<!-- check: shown -->
```text
?- X = 1.
X = 1.
```
