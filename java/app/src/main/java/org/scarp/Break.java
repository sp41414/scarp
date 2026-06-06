package org.scarp;

class Break extends RuntimeException {
    final Token token;
    Break(Token token) {
        super(null, null, false, false);
        this.token = token;
    }
}