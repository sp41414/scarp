package org.scarp;
/*
 * 
 * class AstPrinter implements Expr.Visitor<String> {
 * public static void main(String[] args) {
 * Expr expression = new Expr.Binary(new Expr.Grouping(new Expr.Binary(new
 * Expr.Literal(50),
 * new Token(TokenType.PLUS, "+", null, 1, 1), new Expr.Literal(60))),
 * new Token(TokenType.STAR, "*", null, 1, 1), new Expr.Literal(4));
 * AstPrinter astPrinter = new AstPrinter();
 * System.out.println("Math expression:");
 * System.out.println(astPrinter.print(expression));
 * 
 * expression = new Expr.Ternary(new Expr.Binary(new Expr.Literal(60),
 * new Token(TokenType.GREATER, ">", null, 1, 1), new Expr.Literal(50)), new
 * Expr.Literal("yes"),
 * new Expr.Literal("no"));
 * 
 * System.out.println("Ternary expression:");
 * System.out.println(astPrinter.print(expression));
 * }
 * 
 * String print(Expr expr) {
 * return expr.accept(this);
 * }
 * 
 * @Override
 * public String visitBinaryExpr(Expr.Binary expr) {
 * return parenthesize(expr.operator.lexeme, expr.left, expr.right);
 * }
 * 
 * @Override
 * public String visitGroupingExpr(Expr.Grouping expr) {
 * return parenthesize("group", expr.expression);
 * }
 * 
 * @Override
 * public String visitLiteralExpr(Expr.Literal expr) {
 * if (expr.value == null)
 * return "nil";
 * return expr.value.toString();
 * }
 * 
 * @Override
 * public String visitUnaryExpr(Expr.Unary expr) {
 * return parenthesize(expr.operator.lexeme, expr.right);
 * }
 * 
 * @Override
 * public String visitTernaryExpr(Expr.Ternary expr) {
 * return parenthesize("?", expr.condition, expr.thenBranch, expr.elseBranch);
 * }
 * 
 * private String parenthesize(String name, Expr... exprs) {
 * StringBuilder builder = new StringBuilder();
 * 
 * builder.append("(").append(name);
 * for (Expr expr : exprs) {
 * builder.append(" ").append(expr.accept(this));
 * }
 * builder.append(")");
 * 
 * return builder.toString();
 * }
 * }
 */
