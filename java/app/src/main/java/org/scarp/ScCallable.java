package org.scarp;

import java.util.List;

public interface ScCallable {
    int arity();
    Object call(Interpreter interpreter, List<Object> args);
}
