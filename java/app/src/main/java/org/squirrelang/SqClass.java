package org.squirrelang;

import java.util.List;
import java.util.Map;

public class SqClass implements SqCallable {
    final String name;
    final SqClass base;
    private final Map<String, SqFunction> methods;
    private final Map<String, SqFunction> staticMethods;

    SqClass(String name, SqClass base, Map<String, SqFunction> methods, Map<String, SqFunction> staticMethods) {
        this.base = base;
        this.name = name;
        this.methods = methods;
        this.staticMethods = staticMethods;
    }

    SqFunction findMethod(String name) {
        if (methods.containsKey(name)) {
            return methods.get(name);
        }
        if (base != null) {
            return base.findMethod(name);
        }
        return null;
    }

    SqFunction findStaticMethod(String name) {
        if (staticMethods.containsKey(name)) {
            return staticMethods.get(name);
        }
        if (base != null) {
            return base.findStaticMethod(name);
        }
        return null;
    }

    @Override
    public Object call(Interpreter interpreter, List<Object> args) {
        SqInstance instance = new SqInstance(this);
        SqFunction initializer = findMethod("init");

        if (initializer != null) {
            initializer.bind(instance).call(interpreter, args);
        }

        return instance;
    }

    @Override
    public int arity() {
        SqFunction initializer = findMethod("init");
        if (initializer == null) return 0;
        return initializer.arity();
    }

    @Override
    public String toString() {
        return this.name;
    }
}
