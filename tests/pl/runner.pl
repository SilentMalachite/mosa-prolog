p(1).
p(2).
test(two, p(X), solutions([X = 1, X = 2])).
test(no, p(3), fail).
test(yes, true, true).
test(boom, throw(error(instantiation_error, _)), error(instantiation_error)).
