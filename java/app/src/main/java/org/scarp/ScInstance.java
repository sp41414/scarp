package org.scarp;

import java.util.HashMap;
import java.util.Map;

public class ScInstance {
    private final Map<String, Object> fields = new HashMap<>();
    private ScClass cls;

    ScInstance(ScClass cls) {
        this.cls = cls;
    }

    Object get(Token name) {
        if (fields.containsKey(name.lexeme))
            return fields.get(name.lexeme);
        ScFunction method = cls.findMethod(name.lexeme);
        if (method != null) return method.bind(this);

        throw new RuntimeError(name, "Undefined property '" + name.lexeme + "'.");
    }

    ScClass getCls() { return this.cls; }
    void set(Token name, Object value) {
        fields.put(name.lexeme, value);
    }

    @Override
    public String toString() {
        return "<" + cls.name + " instance>";
    }
}
