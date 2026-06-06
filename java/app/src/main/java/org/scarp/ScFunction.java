package org.scarp;

import java.util.List;

public class ScFunction implements ScCallable {
    final int modifiers;
    private final Stmt.Function declaration;
    private final Environment closure;
    private final boolean isInitializer;
    private ScClass closureClass;

    ScFunction(List<Token> params, List<Stmt> body, Environment closure) {
        this.closure = closure;
        this.declaration = new Stmt.Function(Modifiers.NONE, null, params, body);
        this.isInitializer = false;
        this.modifiers = Modifiers.NONE;
    }

    ScFunction(Stmt.Function declaration, Environment closure, boolean isInitializer, int modifiers) {
        this.closure = closure;
        this.declaration = declaration;
        this.isInitializer = isInitializer;
        this.modifiers = modifiers;
    }

    @Override
    public int arity() {
        return declaration.params.size();
    }

    @Override
    public Object call(Interpreter interpreter, List<Object> args) {
        Environment environment = new Environment(closure);

        for (int i = 0; i < declaration.params.size(); i++) {
            environment.define(declaration.params.get(i).lexeme, args.get(i));
        }

        ScClass enclosingClass = interpreter.currentExecutingClass;
        try {
            interpreter.currentExecutingClass = this.closureClass;
            interpreter.executeBlock(declaration.body, environment);
        } catch (Return value) {
            if (isInitializer) return closure.getAt(0, "self");
            return value.value;
        } finally {
            interpreter.currentExecutingClass = enclosingClass;
        }

        if (isInitializer) return closure.getAt(0, "self");
        return null;
    }

    ScFunction bind(ScInstance instance) {
        Environment environment = new Environment(closure);
        environment.define("self", instance);
        ScFunction bound = new ScFunction(declaration, environment, isInitializer, modifiers);
        bound.setClosureClass(this.closureClass);
        return bound;
    }

    void setClosureClass(ScClass closureClass) {
        this.closureClass = closureClass;
    }

    @Override
    public String toString() {
        if (declaration.name == null) return "<fn>";
        return "<fn " + declaration.name.lexeme + ">";
    }
}
