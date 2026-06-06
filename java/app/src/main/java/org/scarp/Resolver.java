package org.scarp;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Stack;

public class Resolver implements Expr.Visitor<Void>, Stmt.Visitor<Void> {
    private final Interpreter interpreter;
    private final Stack<Map<String, Boolean>> scopes = new Stack<>();
    private final Map<String, Token> used = new HashMap<>();
    private enum FunctionType {
        NONE,
        FUNCTION,
        INITIALIZER,
        METHOD,
    }
    private enum ClassType {
        NONE,
        CLASS,
        SUBCLASS
    }
    private FunctionType currentFunction = FunctionType.NONE;
    private ClassType currentClass = ClassType.NONE;
    private boolean isInStaticMethod = false;
    private boolean isInGetterMethod = false;
    private int loopDepth = 0;

    Resolver(Interpreter interpreter) {
        this.interpreter = interpreter;
    }

    void resolve(List<Stmt> statements) {
        for (Stmt statement : statements) {
            resolve(statement);
        }
    }

    void reportUnused() {
        for (Token unused : used.values()) {
            Scarp.error(unused, "Unused variable.");
        }
        used.clear();
    }

    @Override
    public Void visitBlockStmt(Stmt.Block stmt) {
        beginScope();
        resolve(stmt.statements);
        endScope();
        return null;
    }

    @Override
    public Void visitVarStmt(Stmt.Var stmt) {
        declare(stmt.name);
        if (stmt.initializer != null) {
            resolve(stmt.initializer);
        }
        define(stmt.name);
        return null;
    }

    @Override
    public Void visitFunctionStmt(Stmt.Function stmt) {
        int classModifiers = Modifiers.ALL;
        if ((stmt.modifiers & classModifiers) != 0 && currentClass == ClassType.NONE) {
            Scarp.error(stmt.name, "Cannot use modifier outside of a class.");
        }

        declare(stmt.name);
        define(stmt.name);

        resolveFunction(stmt, FunctionType.FUNCTION);
        return null;
    }

    @Override
    public Void visitClassStmt(Stmt.Class stmt) {
        ClassType enclosingClass = currentClass;
        currentClass = ClassType.CLASS;

        declare(stmt.name);
        define(stmt.name);

        if (stmt.base != null && stmt.base.name.lexeme.equals(stmt.name.lexeme)) {
            Scarp.error(stmt.base.name, "A class can't inherit from itself.\n       Hint: get some sleep");
        }

        if (stmt.base != null) {
            currentClass = ClassType.SUBCLASS;
            resolve(stmt.base);
            beginScope();
            scopes.peek().put("base", true);
        }

        for (Expr.Variable mixin : stmt.mixins) {
            if (mixin.name.lexeme.equals(stmt.name.lexeme))
                Scarp.error(mixin.name, "A class can't have a mixin of itself.\n       Hint: get some sleep");
            resolve(mixin);
        }

        beginScope();
        scopes.peek().put("self", true);

        for (Stmt.Function method : stmt.methods) {
           FunctionType declaration = FunctionType.METHOD;
           if (method.name.lexeme.equals("init")) {
               if ((method.modifiers & Modifiers.ALL) != 0)
                   Scarp.error(method.name, "Initializer cannot have modifiers.");
               declaration = FunctionType.INITIALIZER;
           }

           resolveFunction(method, declaration);
        }

        endScope();

        if (stmt.base != null) endScope();

        currentClass = enclosingClass;
        return null;
    }

    @Override
    public Void visitExpressionStmt(Stmt.Expression stmt) {
        resolve(stmt.expression);
        return null;
    }

    @Override
    public Void visitIfStmt(Stmt.If stmt) {
        resolve(stmt.condition);
        resolve(stmt.thenBranch);
        if (stmt.elseBranch != null) resolve(stmt.elseBranch);
        return null;
    }

    @Override
    public Void visitPrintStmt(Stmt.Print stmt) {
        resolve(stmt.expression);
        return null;
    }

    @Override
    public Void visitReturnStmt(Stmt.Return stmt) {
        if (currentFunction == FunctionType.NONE)
            Scarp.error(stmt.keyword, "Can't return from top-level code.");
        if (stmt.value != null) {
            if (currentFunction == FunctionType.INITIALIZER)
                Scarp.error(stmt.keyword, "Can't return a value from an initializer.");
            resolve(stmt.value);
        }

        return null;
    }

    @Override
    public Void visitWhileStmt(Stmt.While stmt) {
        resolve(stmt.condition);
        loopDepth++;
        resolve(stmt.body);
        loopDepth--;
        return null;
    }

    @Override
    public Void visitBreakStmt(Stmt.Break stmt) {
        if (loopDepth == 0)
            Scarp.error(stmt.token, "Must be in loop to use 'break'.");
        return null;
    }

    @Override
    public Void visitCallExpr(Expr.Call expr) {
        resolve(expr.callee);

        for (Expr argument : expr.args) {
            resolve(argument);
        }

        return null;
    }

    @Override
    public Void visitGroupingExpr(Expr.Grouping expr) {
        resolve(expr.expression);
        return null;
    }

    @Override
    public Void visitLiteralExpr(Expr.Literal expr) {
        return null;
    }

    @Override
    public Void visitLogicalExpr(Expr.Logical expr) {
        resolve(expr.left);
        resolve(expr.right);
        return null;
    }

    @Override
    public Void visitBinaryExpr(Expr.Binary expr) {
        resolve(expr.left);
        resolve(expr.right);
        return null;
    }

    @Override
    public Void visitTernaryExpr(Expr.Ternary expr) {
        resolve(expr.condition);
        resolve(expr.thenBranch);
        resolve(expr.elseBranch);
        return null;
    }

    @Override
    public Void visitVariableExpr(Expr.Variable expr) {
        if (!scopes.isEmpty() && scopes.peek().get(expr.name) == Boolean.FALSE) {
            Scarp.error(expr.name, "Can't read local variable in its own initializer.");
        }
        // Second checks that don't get executed, but are here just in case.
        // First checks are in visitSelfExpr and visitBaseExpr
        if (expr.name.lexeme.equals("self") && isInStaticMethod) {
            Scarp.error(expr.name, "Can't use 'self' inside a static method.");
        }
        if (expr.name.lexeme.equals("base") && isInStaticMethod) {
            Scarp.error(expr.name, "Can't use 'base' inside a static method.");
        }

        resolveLocal(expr, expr.name);
        return null;
    }

    @Override
    public Void visitAssignExpr(Expr.Assign expr) {
        if (isInGetterMethod) Scarp.error(expr.name, "Cannot mutate state inside a getter.");
        resolve(expr.value);
        resolveLocal(expr, expr.name);
        return null;
    }

    @Override
    public Void visitGetExpr(Expr.Get expr) {
        resolve(expr.object);
        return null;
    }

    @Override
    public Void visitSetExpr(Expr.Set expr) {
        if (isInGetterMethod) Scarp.error(expr.name, "Cannot mutate state inside a getter.");
        resolve(expr.object);
        resolve(expr.value);
        return null;
    }

    @Override
    public Void visitSelfExpr(Expr.Self expr) {
        if (isInStaticMethod)
            Scarp.error(expr.keyword,
                    "Cannot use 'self' inside a static method");

        if (currentClass == ClassType.NONE)
            Scarp.error(expr.keyword,
                    "Cannot use 'self' outside of a class.");


        resolveLocal(expr, expr.keyword);
        return null;
    }

    @Override
    public Void visitBaseExpr(Expr.Base expr) {
        if (isInStaticMethod)
            Scarp.error(expr.keyword,
                    "Can't use 'base' inside a static method.");
        if (currentClass == ClassType.NONE)
            Scarp.error(expr.keyword,
                    "Can't use 'base' outside a class.");
        else if (currentClass != ClassType.SUBCLASS)
            Scarp.error(expr.keyword,
                    "Can't use 'base' in a class with no base class.");


        resolveLocal(expr, expr.keyword);
        return null;
    }

    @Override
    public Void visitLambdaExpr(Expr.Lambda expr) {
        resolveLambda(expr);
        return null;
    }

    @Override
    public Void visitUnaryExpr(Expr.Unary expr) {
        resolve(expr.right);
        return null;
    }

    private void resolveLocal(Expr expr, Token name) {
        for (int i = scopes.size() - 1; i >= 0; i--) {
            if (scopes.get(i).containsKey(name.lexeme)) {
                interpreter.resolve(expr, scopes.size() - 1 - i);
                used.remove(name.lexeme);
                return;
            }
        }
    }

    private void resolve(Stmt stmt) {
        stmt.accept(this);
    }

    private void resolve(Expr expr) {
        expr.accept(this);
    }

    private void resolveFunction(Stmt.Function function, FunctionType functionType) {
        FunctionType enclosingFunction = currentFunction;
        int enclosingLoopDepth = loopDepth;
        boolean wasInStaticMethod = isInStaticMethod;
        boolean wasInGetterMethod = isInGetterMethod;

        currentFunction = functionType;
        isInStaticMethod = (function.modifiers & Modifiers.STATIC) != 0;
        isInGetterMethod = (function.modifiers & Modifiers.GETTER) != 0;
        loopDepth = 0;

        beginScope();
        for (Token param : function.params) {
            declare(param);
            define(param);
        }
        resolve(function.body);
        endScope();
        loopDepth = enclosingLoopDepth;
        currentFunction = enclosingFunction;
        isInStaticMethod = wasInStaticMethod;
        isInGetterMethod = wasInGetterMethod;
    }

    private void resolveLambda(Expr.Lambda lambda) {
        resolveFunction(new Stmt.Function(Modifiers.NONE, null, lambda.params, lambda.body), FunctionType.FUNCTION);
    }

    private void beginScope() {
        scopes.push(new HashMap<String, Boolean>());
    }

    private void endScope() {
        scopes.pop();
    }

    private void declare(Token name) {
        if (scopes.isEmpty()) return;
        Map<String, Boolean> scope = scopes.peek();
        if (scope.containsKey(name.lexeme)) {
            Scarp.error(name, "Already a variable with this name in this scope.");
        }
        scope.put(name.lexeme, false);
    }

    private void define(Token name) {
        if (scopes.isEmpty()) return;
        scopes.peek().put(name.lexeme, true);
        used.put(name.lexeme, name);
    }
}
