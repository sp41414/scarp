package org.scarp;

import java.util.List;
import java.util.Map;

public class ScClass implements ScCallable {
  final String name;
  final ScClass base;
  private final Map<String, ScFunction> methods;
  private final Map<String, ScFunction> staticMethods;

  ScClass(
      String name,
      ScClass base,
      Map<String, ScFunction> methods,
      Map<String, ScFunction> staticMethods) {
    this.base = base;
    this.name = name;
    this.methods = methods;
    this.staticMethods = staticMethods;
  }

  public Map<String, ScFunction> getMethods() {
    return methods;
  }

  ScFunction findMethod(String name) {
    if (methods.containsKey(name)) {
      return methods.get(name);
    }
    if (base != null) {
      return base.findMethod(name);
    }
    return null;
  }

  ScFunction findStaticMethod(String name) {
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
    ScInstance instance = new ScInstance(this);
    ScFunction initializer = findMethod("init");

    if (initializer != null) {
      initializer.bind(instance).call(interpreter, args);
    }

    return instance;
  }

  @Override
  public int arity() {
    ScFunction initializer = findMethod("init");
    if (initializer == null) return 0;
    return initializer.arity();
  }

  @Override
  public String toString() {
    return "<class " + this.name + ">";
  }
}
