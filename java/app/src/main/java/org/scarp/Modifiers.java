package org.scarp;

public class Modifiers {
    public static final int NONE = 0;
    public static final int STATIC = 1;
    public static final int PRIVATE = 1 << 1;
    public static final int GETTER = 1 << 2;
    public static final int ALL = STATIC | PRIVATE | GETTER;
}
