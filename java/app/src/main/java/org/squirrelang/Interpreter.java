package org.squirrelang;

import static org.squirrelang.TokenType.*;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class Interpreter implements Expr.Visitor<Object>, Stmt.Visitor<Void> {
    private boolean isRepl;
    final Environment globals = new Environment();
    private Environment environment = globals;
    private final Map<Expr, Integer> locals = new HashMap<>();

    Interpreter() {
        globals.define(
                "clock",
                new SqCallable() {
                    @Override
                    public int arity() {
                        return 0;
                    }

                    @Override
                    public Object call(Interpreter interpreter, List<Object> args) {
                        return (double)System.currentTimeMillis() / 1000.0;
                    }

                    @Override
                    public String toString() {
                        return "<native fn>";
                    }
                }
        );
    }

    void interpret(List<Stmt> statements, boolean isRepl) {
        this.isRepl = isRepl;

        try {
            for (Stmt statement : statements) {
                execute(statement);
            }
        } catch (RuntimeError error) {
            Squirrelang.runtimeError(error);
        } catch (Break berror) {
            // This error is caught in the resolver, but just in case...
            Squirrelang.runtimeError(new RuntimeError(berror.token, "Must be inside loop to use 'break'."));
        }
    }

    void executeBlock(List<Stmt> statements, Environment environment) {
        Environment previous = this.environment;
        try {
            this.environment = environment;
            for (Stmt statement : statements) {
                execute(statement);
            }
        } finally {
            this.environment = previous;
        }
    }

    void resolve(Expr expr, int depth) {
        locals.put(expr, depth);
    }

    private String stringify(Object value) {
        if (value == null)
            return "nil";
        if (value instanceof Double) {
            String text = value.toString();
            if (text.endsWith(".0")) {
                text = text.substring(0, text.length() - 2);
            }
            return text;
        }

        return value.toString();
    }

    @Override
    public Void visitExpressionStmt(Stmt.Expression stmt) {
        Object value = evaluate(stmt.expression);
        if (isRepl) System.out.println(stringify(value));
        return null;
    }

    @Override
    public Void visitPrintStmt(Stmt.Print stmt) {
        Object value = evaluate(stmt.expression);
        System.out.println(stringify(value));
        return null;
    }

    @Override
    public Void visitIfStmt(Stmt.If stmt) {
        if (isTruthy(evaluate(stmt.condition))) {
            execute(stmt.thenBranch);
        } else if (stmt.elseBranch != null) {
            execute(stmt.elseBranch);
        }
        return null;
    }

    @Override
    public Void visitWhileStmt(Stmt.While stmt) {
        while (isTruthy(evaluate(stmt.condition))) {
            try {
                execute(stmt.body);
            } catch (Break b) {
                break;
            }
        }
        return null;
    }

    @Override
    public Void visitBreakStmt(Stmt.Break stmt) {
        throw new Break(stmt.token);
    }

    @Override
    public Void visitVarStmt(Stmt.Var stmt) {
        if (stmt.initializer != null) {
            environment.define(stmt.name.lexeme, evaluate(stmt.initializer));
        } else {
            environment.define(stmt.name.lexeme);
        }
        return null;
    }

    @Override
    public Void visitFunctionStmt(Stmt.Function stmt) {
        SqFunction function = new SqFunction(stmt, environment);
        environment.define(stmt.name.lexeme, function);
        return null;
    }

    @Override
    public Void visitReturnStmt(Stmt.Return stmt) {
        Object value = null;
        if (stmt.value != null) value = evaluate(stmt.value);

        throw new Return(value);
    }

    @Override
    public Void visitBlockStmt(Stmt.Block stmt) {
        executeBlock(stmt.statements, new Environment(environment));
        return null;
    }

    @Override
    public Object visitVariableExpr(Expr.Variable expr) {
        return lookupVariable(expr.name, expr);
    }

    @Override
    public Object visitAssignExpr(Expr.Assign expr) {
        Object value = evaluate(expr.value);

        Integer distance = locals.get(expr);
        if (distance != null) {
            environment.assignAt(distance, expr.name, value);
        } else {
            globals.assign(expr.name, value);
        }
        return value;
    }

    @Override
    public Object visitLiteralExpr(Expr.Literal expr) {
        return expr.value;
    }

    @Override
    public Object visitGroupingExpr(Expr.Grouping expr) {
        return evaluate(expr.expression);
    }

    @Override
    public Object visitUnaryExpr(Expr.Unary expr) {
        Object right = evaluate(expr.right);

        switch (expr.operator.type) {
            case PLUS:
                return +(double) right;
            case MINUS:
                checkNumberOperand(expr.operator, right);
                return -(double) right;
            case BANG:
                return !isTruthy(right);
            // cant do NOT on doubles, so make it LONG, apply NOT, then back to double again
            case TILDE:
                return (double)~toLong(right);
        }
        ;
        return null;
    }

    @Override
    public Object visitBinaryExpr(Expr.Binary expr) {
        Object left = evaluate(expr.left);
        Object right = evaluate(expr.right);

        switch (expr.operator.type) {
            case MINUS:
                return (double) left - (double) right;
            case SLASH:
                checkNumberOperands(expr.operator, left, right);
                if ((double) right == 0.0) {
                    throw new RuntimeError(expr.operator, "Division by zero");
                }
                return (double) left / (double) right;
            case STAR:
                checkNumberOperands(expr.operator, left, right);
                return (double) left * (double) right;
            case PLUS:
                if (left instanceof Double && right instanceof Double) {
                    return (double) left + (double) right;
                }
                if (left instanceof String) {
                    return left + stringify(right);
                }
                throw new RuntimeError(expr.operator, "Operands must be two numbers or a string and any type.");
            case GREATER:
            case GREATER_EQUAL:
            case LESS:
            case LESS_EQUAL:
                checkNumberOrStringOperands(expr.operator, left, right);
                return binaryComparison(expr.operator, left, right);
            case EQUAL_EQUAL:
                return isEqual(left, right);
            case BANG_EQUAL:
                return !isEqual(left, right);
            case XOR:
                checkNumberOperands(expr.operator, left, right);
                return binaryBitwiseOperation(left, XOR, right);
            case AND:
                checkNumberOperands(expr.operator, left, right);
                return binaryBitwiseOperation(left, AND, right);
            case OR:
                checkNumberOperands(expr.operator, left, right);
                return binaryBitwiseOperation(left, OR, right);
            case SHIFT_LEFT:
                checkNumberOperands(expr.operator, left, right);
                return binaryBitwiseOperation(left, SHIFT_LEFT, right);
            case SHIFT_RIGHT:
                checkNumberOperands(expr.operator, left, right);
                return binaryBitwiseOperation(left, SHIFT_RIGHT, right);
            case SHIFT_RIGHT_UNSIGNED:
                checkNumberOperands(expr.operator, left, right);
                return binaryBitwiseOperation(left, SHIFT_RIGHT_UNSIGNED, right);
        }
        return null;
    }

    @Override
    public Object visitTernaryExpr(Expr.Ternary expr) {
        Object condition = evaluate(expr.condition);
        if (isTruthy(condition))
            return evaluate(expr.thenBranch);
        else
            return evaluate(expr.elseBranch);
    }

    @Override
    public Object visitLogicalExpr(Expr.Logical expr) {
        Object left = evaluate(expr.left);

        if (expr.operator.type == OR) {
            if (isTruthy(left)) return left;
        } else {
            if (!isTruthy(left)) return left;
        }

        return evaluate(expr.right);
    }

    @Override
    public Object visitCallExpr(Expr.Call expr) {
        Object callee = evaluate(expr.callee);

        List<Object> args = new ArrayList<>();
        for (Expr arg : expr.args) {
            args.add(evaluate(arg));
        }

        if (!(callee instanceof SqCallable function)) {
            throw new RuntimeError(expr.paren, "Can only call functions and classes.");
        }

        if (args.size() != function.arity()) {
            throw new RuntimeError(expr.paren, "Expected "
                    + function.arity()
                    + " arguments but got "
                    + args.size() + ".");
        }
        return function.call(this, args);
    }

    @Override
    public Object visitLambdaExpr(Expr.Lambda expr) {
        return new SqFunction(expr.params, expr.body, environment);
    }

    private Object evaluate(Expr expr) {
        return expr.accept(this);
    }

    private Object lookupVariable(Token name, Expr expr) {
        Integer distance = locals.get(expr);
        if (distance != null) {
            return environment.getAt(distance, name.lexeme);
        } else {
            return globals.get(name);
        }
    }

    private void execute(Stmt stmt) {
        stmt.accept(this);
    }

    private boolean isTruthy(Object object) {
        if (object == null)
            return false;
        if (object instanceof Boolean)
            return (boolean) object;
        return true;
    }

    private boolean isEqual(Object a, Object b) {
        if (a == null && b == null)
            return true;
        if (a == null)
            return false;

        return a.equals(b);
    }

    private boolean binaryComparison(Token op, Object left, Object right) {
        // lexicographical comparison
        if (left instanceof String l && right instanceof String r) {
            int cmp = l.compareTo(r);
            return switch (op.type) {
                case GREATER -> cmp > 0;
                case GREATER_EQUAL -> cmp >= 0;
                case LESS -> cmp < 0;
                case LESS_EQUAL -> cmp <= 0;
                default -> throw new RuntimeError(op, "Unexpected operator.");
            };
        }

        // string length comparison (to another double)
        // or double to double comparison
        double l = toDouble(left);
        double r = toDouble(right);

        return switch (op.type) {
            case GREATER -> l > r;
            case GREATER_EQUAL -> l >= r;
            case LESS_EQUAL -> l <= r;
            case LESS -> l < r;
            default -> throw new RuntimeError(op, "Unexpected error in parsing token type.");
        };
    }

    private double binaryBitwiseOperation(Object a, TokenType op, Object b) {
        long longA = toLong(a);
        long longB = toLong(b);

        return (double) switch (op) {
            case XOR -> longA ^ longB;
            case AND -> longA & longB;
            case OR -> longA | longB;
            case SHIFT_RIGHT -> longA >> longB;
            case SHIFT_LEFT -> longA << longB;
            case SHIFT_RIGHT_UNSIGNED -> longA >>> longB;
            default -> 0L;
        };
    }

    private long toLong(Object val) {
        if (val instanceof Double d)
            return d.longValue();
        if (val instanceof Boolean b)
            return b ? 1L : 0L;
        return 0L;
    }

    private double toDouble(Object val) {
        if (val instanceof Double d)
            return d;
        if (val instanceof String s)
            return s.length();
        return 0.0;
    }

    private void checkNumberOperand(Token operator, Object operand) {
        if (operand instanceof Double)
            return;
        throw new RuntimeError(operator, "Operand must be a number.");
    }

    private void checkNumberOrStringOperands(Token operator, Object left, Object right) {
        if (left instanceof Double && right instanceof Double)
            return;
        if (left instanceof String || right instanceof String)
            return;
        throw new RuntimeError(operator, "Operands must be numbers or strings.");
    }

    private void checkNumberOperands(Token operator, Object left, Object right) {
        if (left instanceof Double && right instanceof Double)
            return;
        throw new RuntimeError(operator, "Operands must be numbers.");
    }
}
