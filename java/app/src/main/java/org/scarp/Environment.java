package org.scarp;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

public class Environment {
    final Environment enclosing;
    private final Map<String, Object> values = new HashMap<>();
    private final Set<String> constants = new HashSet<>();
    private static final Object UNINITIALIZED = new Object();

    Environment() {
        enclosing = null;
    }

    Environment(Environment enclosing) {
        this.enclosing = enclosing;
    }

    void define(String name) {
        values.put(name, UNINITIALIZED);
    }

    void define(String name, Object value, boolean isConst) {
        if (isConst)
            constants.add(name);

        values.put(name, value);
    }

    void assign(Token name, Object value) {
        if (constants.contains(name.lexeme))
            throw new RuntimeError(name, "Cannot reassign to a constant variable");

        if (values.containsKey(name.lexeme)) {
            values.put(name.lexeme, value);
            return;
        }
        if (enclosing != null) {
            enclosing.assign(name, value);
            return;
        }
        throw new RuntimeError(name, "Undefined variable '" + name.lexeme + "'.");
    }

    void assignAt(int distance, Token name, Object value) {
        ancestor(distance).values.put(name.lexeme, value);
    }

    Object get(Token name) {
        if (values.containsKey(name.lexeme)) {
            Object value = values.get(name.lexeme);
            if (value == UNINITIALIZED) {
                throw new RuntimeError(name, "Cannot reference uninitialized variable '" + name.lexeme + "'.");
            }
            return value;
        }
        if (enclosing != null) {
            return enclosing.get(name);
        }
        throw new RuntimeError(name, "Undefined variable '" + name.lexeme + "'.");
    }

    Object getAt(int distance, String name) {
        return ancestor(distance).values.get(name);
    }

    Environment ancestor(int distance) {
        Environment environment = this;
        for (int i = 0; i < distance; i++) {
            environment = environment.enclosing;
        }
        return environment;
    }
}
