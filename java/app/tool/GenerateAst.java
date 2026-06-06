package org.scarp.tool;

import java.io.IOException;
import java.io.PrintWriter;
import java.util.Arrays;
import java.util.List;

public class GenerateAst {
    public static void main(String[] args) throws IOException {
        if (args.length != 1) {
            System.out.println("Usage: generate_ast <output directory>");
            System.exit(64);
        }
        String outputDir = args[0];
        defineAst(outputDir, "Expr", Arrays.asList(
                "Assign   : Token name, Expr value",
                "Binary   : Expr left, Token operator, Expr right",
                "Lambda   : List<Token> params, List<Stmt> body",
                "Call     : Expr callee, Token paren, List<Expr> args",
                "Grouping : Expr expression",
                "Literal  : Object value",
                "Logical  : Expr left, Token operator, Expr right",
                "Unary    : Token operator, Expr right",
                "Ternary  : Expr condition, Expr thenBranch, Expr elseBranch",
                "Variable : Token name",
                "Get      : Expr object, Token name",
                "Set      : Expr object, Token name, Expr value",
                "Self     : Token keyword",
                "Base     : Token keyword, Token method"));
        defineAst(outputDir, "Stmt", Arrays.asList(
                "Block      : List<Stmt> statements",
                "Expression : Expr expression",
                "Print      : Expr expression",
                "Return     : Token keyword, Expr value",
                "Var        : Token name, Expr initializer",
                "Function   : int modifiers, Token name, List<Token> params, List<Stmt> body",
                "Class      : Token name, Expr.Variable base, List<Expr.Variable> mixins, List<Stmt.Function> methods",
                "If         : Expr condition, Stmt thenBranch, Stmt elseBranch",
                "While      : Expr condition, Stmt body",
                "Break      : Token token"
                ));
    }

    private static void defineAst(String outputDir, String baseName, List<String> types) throws IOException {
        String path = outputDir + "/" + baseName + ".java";
        PrintWriter fileWriter = new PrintWriter(path, "UTF-8");

        fileWriter.println("package org.scarp;");
        fileWriter.println();
        fileWriter.println("import java.util.List;");
        fileWriter.println();
        fileWriter.println("abstract class " + baseName + "{ ");
        defineVisitor(fileWriter, baseName, types);
        for (String type : types) {
            String className = type.split(":")[0].trim();
            String fields = type.split(":")[1].trim();

            defineTypes(baseName, fileWriter, className, fields);
        }
        fileWriter.println();
        fileWriter.println("  abstract <R> R accept(Visitor<R> visitor);");
        fileWriter.println("}");
        fileWriter.close();
    }

    private static void defineTypes(String baseName, PrintWriter fileWriter, String className,
            String fields) {
        fileWriter.println("  static class " + className + " extends " + baseName + "{ ");
        fileWriter.println("    " + className + "(" + fields + ") { ");

        String[] fieldList = fields.split(", ");
        for (String field : fieldList) {
            // skip the type definition
            field = field.split(" ")[1];
            fileWriter.println("      this." + field + " = " + field + ";");
        }
        fileWriter.println("    }");

        fileWriter.println();
        fileWriter.println("    @Override");
        fileWriter.println("    <R> R accept(Visitor<R> visitor) {");
        fileWriter.println("      return visitor.visit" +
                className + baseName + "(this);");
        fileWriter.println("    }");

        fileWriter.println();
        for (String field : fieldList) {
            fileWriter.println("    final " + field + ";");
        }
        fileWriter.println("  }");
    }

    private static void defineVisitor(PrintWriter fileWriter, String baseName, List<String> types) {
        fileWriter.println("  interface Visitor<R> {");
        for (String type : types) {
            String typeName = type.split(":")[0].trim();
            // e.g. R visitBinaryExpr(Binary expr)
            fileWriter.println(
                    "    R visit" + typeName + baseName + "(" + typeName + " " + baseName.toLowerCase() + ");");
        }
        fileWriter.println("  }");
    }
}
