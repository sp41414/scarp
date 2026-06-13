package org.scarp;

import static org.scarp.TokenType.*;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

class Scanner {
    private final String source;
    private final List<Token> tokens = new ArrayList<>();
    private static final Map<String, TokenType> keywords;
    private int start = 0;
    private int current = 0;
    private int line = 1;
    private int column = 1;
    private int columnStart = 1;
    private int multiLineCommentDepth = 0;

    static {
        keywords = new HashMap<>();
        keywords.put("class", CLASS);
        keywords.put("else", ELSE);
        keywords.put("false", FALSE);
        keywords.put("for", FOR);
        keywords.put("fn", FUNCTION);
        keywords.put("if", IF);
        keywords.put("nil", NIL);
        keywords.put("print", PRINT);
        keywords.put("ret", RETURN);
        keywords.put("base", BASE);
        keywords.put("with", WITH);
        keywords.put("static", STATIC);
        keywords.put("private", PRIVATE);
        keywords.put("self", SELF);
        keywords.put("true", TRUE);
        keywords.put("let", LET);
        keywords.put("while", WHILE);
        keywords.put("break", BREAK);
    }

    Scanner(String source) {
        this.source = source;
    }

    List<Token> scanTokens() {
        while (!isAtEnd()) {
            start = current;
            columnStart = column;
            scanToken();
        }

        tokens.add(new Token(EOF, "", null, line, column - 1));
        return tokens;
    }

    private boolean isAtEnd() {
        return current >= source.length();
    }

    private char advance() {
        column++;
        return source.charAt(current++);
    }

    private boolean isDigit(char c) {
        return c >= '0' && c <= '9';
    }

    private boolean match(char expected) {
        if (isAtEnd())
            return false;
        if (source.charAt(current) != expected)
            return false;
        column++;
        current++;
        return true;
    }

    private char peek() {
        if (isAtEnd())
            return '\0';
        return source.charAt(current);
    }

    private char peekNext() {
        if (current + 1 >= source.length())
            return '\0';
        return source.charAt(current + 1);
    }

    private boolean isAlpha(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    private boolean isAlphaNumeric(char c) {
        return isAlpha(c) || isDigit(c);
    }

    private void addToken(TokenType type) {
        addToken(type, null);
    }

    private void addToken(TokenType type, Object literal) {
        String text = source.substring(start, current);
        tokens.add(new Token(type, text, literal, line, columnStart));
    }

    private void scanToken() {
        char c = advance();
        switch (c) {
            case '(':
                addToken(LEFT_PAREN);
                break;
            case ')':
                addToken(RIGHT_PAREN);
                break;
            case '{':
                addToken(LEFT_BRACE);
                break;
            case '}':
                addToken(RIGHT_BRACE);
                break;
            case ',':
                addToken(COMMA);
                break;
            case '.':
                addToken(DOT);
                break;
            case '-':
                addToken(MINUS);
                break;
            case '+':
                addToken(PLUS);
                break;
            case ';':
                addToken(SEMICOLON);
                break;
            case '*':
                addToken(STAR);
                break;
            case '?':
                addToken(QUESTION);
                break;
            case ':':
                addToken(COLON);
                break;
            case '^':
                addToken(XOR);
                break;
            case '~':
                addToken(TILDE);
                break;
            case '!':
                addToken(match('=') ? BANG_EQUAL : BANG);
                break;
            case '=':
                addToken(match('=') ? EQUAL_EQUAL : EQUAL);
                break;
            case '<':
                if (match('<')) {
                    addToken(SHIFT_LEFT);
                } else if (match('=')) {
                    addToken(LESS_EQUAL);
                } else {
                    addToken(LESS);
                }
                break;
            case '>':
                if (match('>')) {
                    if (match('>')) {
                        addToken(SHIFT_RIGHT_UNSIGNED);
                    } else {
                        addToken(SHIFT_RIGHT);
                    }
                } else if (match('=')) {
                    addToken(GREATER_EQUAL);
                } else {
                    addToken(GREATER);
                }
                break;
            case '&':
                addToken(match('&') ? AND_AND : AND);
                break;
            case '|':
                addToken(match('|') ? OR_OR : OR);
                break;
            case '/':
                if (match('/')) {
                    // A comment goes until the end of the line.
                    while (peek() != '\n' && !isAtEnd())
                        advance();
                } else if (match('*')) {
                    multiLineCommentDepth++;
                    multiLineComment();
                } else {
                    addToken(SLASH);
                }
                break;
            case ' ':
            case '\r':
            case '\t':
                // Ignore whitespace.
                break;
            case '\n':
                line++;
                column = 1;
                break;
            case '"':
                string();
                break;
            default:
                if (isDigit(c)) {
                    while (isDigit(peek()))
                        advance();
                    if (peek() == '.' && isDigit(peekNext())) {
                        advance();
                        while (isDigit(peek()))
                            advance();
                    }
                    addToken(NUMBER, Double.parseDouble(source.substring(start, current)));
                } else if (isAlpha(c)) {
                    identifier();
                } else {
                    Scarp.error(line, column, "Unexpected character: " + c);
                }
        }
    }

    private void multiLineComment() {
        while (multiLineCommentDepth != 0) {
            if (isAtEnd()) {
                Scarp.error(line, column, "Unterminated multi-line comment.");
                return;
            }

            if (peek() == '\n') {
                line++;
                column = 1;
            }

            if (peek() == '*' && peekNext() == '/') {
                multiLineCommentDepth--;
                advance();
            } else if (peek() == '/' && peekNext() == '*') {
                multiLineCommentDepth++;
                advance();
            }
            advance();
        }
    }

    private void string() {
        while (!isAtEnd() && peek() != '"') {
            if (peek() == '\n') {
                line++;
                column = 1;
            }
            advance();
        }

        if (isAtEnd()) {
            Scarp.error(line, column, "Unterminated string.");
            return;
        }

        advance();
        String value = source.substring(start + 1, current - 1);
        addToken(STRING, value);
    }

    private void identifier() {
        while (isAlphaNumeric(peek()))
            advance();

        String text = source.substring(start, current);
        TokenType type = keywords.get(text);
        if (type == null)
            type = IDENTIFIER;
        addToken(type);
    }
}
