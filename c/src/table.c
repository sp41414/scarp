#include "table.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include <string.h>

void initTable(Table *table) {
  table->capacity = 0;
  table->count = 0;
  table->entries = NULL;
}

void freeTable(Table *table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  initTable(table);
}

static uint32_t hashNumber(double key) {
  // normalize -0.0 to 0.0
  if (key == 0.0)
    key = 0.0;

  // cast double to uint64
  union {
    double d;
    uint64_t bits;
  } cast;
  cast.d = key;

  // thomas wang's 64 bit mix function
  cast.bits += ~(cast.bits << 32);
  cast.bits ^= (cast.bits >> 22);
  cast.bits += ~(cast.bits << 13);
  cast.bits ^= (cast.bits >> 8);
  cast.bits += (cast.bits << 3);
  cast.bits ^= (cast.bits >> 15);
  cast.bits += ~(cast.bits << 27);
  cast.bits ^= (cast.bits >> 31);

  return (uint32_t)cast.bits;
}

static uint32_t hashValue(Value key) {
  switch (key.type) {
  case VAL_NIL:
    return 0;
  case VAL_BOOL:
    return AS_BOOL(key) ? 1 : 0;
  case VAL_NUMBER:
    return hashNumber(AS_NUMBER(key));
  case VAL_OBJ:
    if (IS_STRING(key)) {
      return AS_STRING(key)->hash;
    }
    return (uint32_t)(uintptr_t)AS_OBJ(key);
  default:
    return 0;
  }
}

static Entry *findEntry(Entry *entries, int capacity, Value key) {
  uint32_t idx = hashValue(key) % capacity;
  Entry *tombstone = NULL;

  for (;;) {
    Entry *entry = &entries[idx];
    if (IS_NIL(entry->key)) {
      if (IS_NIL(entry->value)) {
        return tombstone != NULL ? tombstone : entry;
      } else {
        if (tombstone == NULL)
          tombstone = entry;
      }
    } else if (valuesEqual(entry->key, key)) {
      return entry;
    }

    idx = (idx + 1) % capacity;
  }
}

bool tableGet(Table *table, Value key, Value *value) {
  if (table->count == 0)
    return false;

  Entry *entry = findEntry(table->entries, table->capacity, key);
  if (IS_NIL(entry->key))
    return false;

  *value = entry->value;
  return true;
}

static void adjustCapacity(Table *table, int capacity) {
  Entry *entries = ALLOCATE(Entry, capacity);
  for (int i = 0; i < capacity; i++) {
    entries[i].key = NIL_VAL;
    entries[i].value = NIL_VAL;
  }

  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    Entry *entry = &table->entries[i];
    if (IS_NIL(entry->key))
      continue;

    Entry *dest = findEntry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
    table->count++;
  }

  FREE_ARRAY(Entry, table->entries, table->capacity);
  table->entries = entries;
  table->capacity = capacity;
}

bool tableSet(Table *table, Value key, Value value) {
  if (table->count + 1 > table->capacity * TABLE_LOAD_FACTOR) {
    int capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(table, capacity);
  }

  Entry *entry = findEntry(table->entries, table->capacity, key);
  bool isNewKey = IS_NIL(entry->key);
  if (isNewKey && IS_NIL(entry->value))
    table->count++;

  entry->key = key;
  entry->value = value;
  return isNewKey;
}

bool tableDelete(Table *table, Value key) {
  if (table->count == 0)
    return false;

  Entry *entry = findEntry(table->entries, table->capacity, key);
  if (IS_NIL(entry->key))
    return false;

  entry->key = NIL_VAL;
  entry->value = BOOL_VAL(true);
  return true;
}

void tableAddAll(Table *from, Table *to) {
  for (int i = 0; i < from->capacity; i++) {
    Entry *entry = &from->entries[i];
    if (!IS_NIL(entry->key))
      tableSet(to, entry->key, entry->value);
  }
}

ObjString *tableFindString(Table *table, const char *chars, int length,
                           uint32_t hash) {
  if (table->count == 0)
    return NULL;

  int idx = hash % table->capacity;
  for (;;) {
    Entry *entry = &table->entries[idx];
    if (IS_NIL(entry->key)) {
      if (IS_NIL(entry->value))
        return NULL;
    } else if (IS_STRING(entry->key)) {
      ObjString *string = AS_STRING(entry->key);
      if (string->length == length && string->hash == hash &&
          memcmp(string->chars, chars, length) == 0) {
        return string;
      }
    }

    idx = (idx + 1) % table->capacity;
  }
}
