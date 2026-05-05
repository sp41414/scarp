package org.squirrelang;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

public class Squirrelang {
  private static final String RESET = "\033[0m";
  private static final String RED = "\033[1;31m";
  private static final String BLUE = "\033[1;34m";
  private static final Interpreter interpreter = new Interpreter();
  static boolean hadError = false;
  static boolean hadRuntimeError = false;
  static String fileName = "<stdin>";
  static String source = "";

  public static void main(String[] args) throws IOException {
    if (args.length > 1) {
      System.out.println("Usage: java Squirrelang <file>");
      System.exit(64);
    } else if (args.length == 1) {
      runFile(args[0]);
    } else {
      runPrompt();
    }
  }

  private static void runFile(String pathName) throws IOException {
    fileName = pathName.substring(pathName.lastIndexOf("/") + 1);
    source = Files.readString(Paths.get(pathName), Charset.defaultCharset());
    run(source,false);
    if (hadError)
      System.exit(65);
    if (hadRuntimeError)
      System.exit(70);
  }

  private static void runPrompt() throws IOException {
    InputStreamReader input = new InputStreamReader(System.in);
    BufferedReader reader = new BufferedReader(input);

    for (;;) {
      System.out.print("> ");
      source = reader.readLine();
      if (source == null)
        break;
      if (source.equalsIgnoreCase("exit"))
        System.exit(0);
      run(source,true);
      hadError = false;
    }
  }

  private static void run(String source, boolean isRepl) {
    Scanner scanner = new Scanner(source);
    List<Token> tokens = scanner.scanTokens();

    Parser parser = new Parser(tokens);
    List<Stmt> statements = parser.parse();

    if (hadError || statements == null)
      return;

    Resolver resolver = new Resolver(interpreter);
    resolver.resolve(statements);

    if (hadError)
      return;

    interpreter.interpret(statements, isRepl);
  }

  /**
   * Reports an error in format:
   * error: message
   * --> file:line:col
   * |
   */
  static void error(int line, int column, String message) {
    report(line, column, "", message);
  }

  /**
   * Reports an error in format:
   * error: message
   * --> file:line:col
   * |
   * l | token lexeme
   * ^^^^^
   */
  static void error(Token token, String message) {
    if (token.type == TokenType.EOF) {
      report(token.line, token.column, null, message);
    } else {
      report(token.line, token.column, token.lexeme, message);
    }
  }

  static void runtimeError(RuntimeError err) {
    hadRuntimeError = true;
    error(err.token, err.getMessage());
  }

  private static void report(int line, int column, String where, String message) {
    String pad = " ".repeat(String.valueOf(line).length());
    String sourceLine = getLine(line);

    System.err.println(RED + "error" + RESET + ": " + message);
    System.err.println(BLUE + pad + " --> " + RESET + fileName + ":" + line + ":" + column);
    System.err.println(BLUE + pad + " |" + RESET);
    System.err.println(BLUE + line + " | " + RESET + sourceLine);
    System.err.println(BLUE + pad + " | " + RESET + " ".repeat(column - 1) + RED
        + "^".repeat(where == null || where.isEmpty() ? 1 : where.length()) + RESET);
  }

  private static String getLine(int line) {
    int currLine = 1;
    int start = 0;

    for (int i = 0; i < source.length(); i++) {
      if (source.charAt(i) == '\n') {
        if (currLine == line)
          return source.substring(start, i);
        currLine++;
        start = i + 1;
      }
    }

    return source.substring(start);
  }
}
