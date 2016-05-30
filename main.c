#include <readline/history.h>
#include <readline/readline.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* begin utilities */
void reverse(char s[])
{
  int i, j;
  char c;

  for (i = 0, j = strlen(s) - 1; i<j; i++, j--)
  {
    c = s[i];
    s[i] = s[j];
    s[j] = c;
  }
}

char* itoa(int n, char s[])
{
  int i, sign;
  if ((sign = n) < 0) n = -n;

  i = 0;
  do
  {
    s[i++] = n % 10 + '0';
  } while ((n /= 10) > 0);

  if (sign < 0) s[i++] = '-';

  s[i] = '\0';

  reverse(s);

  return s;
}
/* end utilities */

/* begin types */
struct SExpression;
struct String;
struct Symbol;

enum Tag;
union Instance;
struct Object;

struct Environment;

typedef struct SExpression SExpression;
typedef struct String String;
typedef struct Symbol Symbol;

typedef enum Tag Tag;
typedef union Instance Instance;
typedef struct Object Object;

typedef struct Environment Environment;

struct SExpression
{
  Object* first;
  Object* rest; // must be an s-expression
};

struct String
{
  char* characters;
};

struct Symbol
{
  char* name;
};

enum Tag
{
  CLOSURE,
  INTEGER,
  S_EXPRESSION,
  STRING,
  SYMBOL
};

union Instance
{
  Object* (*closure)(Environment* environment, Object* arguments);
  int32_t integer;
  SExpression sExpression;
  String string;
  Symbol symbol;
};

struct Object
{
  size_t referenceCount;
  Tag tag;
  Instance instance;
};

struct Environment
{
  size_t referenceCount;
  Object* key; // Must be a symbol
  Object* value;
  Environment* next;
};

struct ParseResult;
typedef struct ParseResult ParseResult;
struct ParseResult
{
  bool succeeded;
  Object* result;
  char* remaining;
};
/* end types */

/* begin constructors */
Object* constructObject(Tag tag)
{
  Object* result = malloc(sizeof(Object));
  result->referenceCount = 1;
  result->tag = tag;
  return result;
}

Environment* constructEnvironment(Object* key, Object* value, Environment* next)
{
  Environment* result = malloc(sizeof(Environment));
  result->referenceCount = 1;
  result->key = key;
  result->value = value;
  result->next = next;
  return result;
}
/* end constructors */

/* begin dereferencers */
Object* rereferenceObject(Object* self)
{
  if(self == NULL) return self;
  self->referenceCount++;
  return self;
}

void dereferenceObject(Object* self)
{
  if(self == NULL) return;

  self->referenceCount--;

  if(self->referenceCount > 0) return;

  switch(self->tag)
  {
    case CLOSURE:
    case INTEGER:
      free(self);
      break;

    case S_EXPRESSION:
      dereferenceObject(self->instance.sExpression.first);
      dereferenceObject(self->instance.sExpression.rest);
      free(self);
      break;

    case STRING:
      free(self->instance.string.characters);
      free(self);
      break;

    case SYMBOL:
      free(self->instance.symbol.name);
      free(self);
      break;

    default:
      printf("Dereference object called with unexpected type");
      exit(EXIT_FAILURE);
  }
}
/* end dereferencers */

/* begin parser helpers */
bool isWhitespaceCharacter(char character)
{
  switch(character)
  {
    case ' ':
    case '\t':
    case '\n':
      return true;

    default:
      return false;
  }
}

char* consumeWhitespace(char* source)
{
  while(isWhitespaceCharacter(*source)) source++;
  return source;
}

bool isDigit(char character) { return '0' <= character && character <= '9'; }
int32_t toDigit(char character) { return character - '0'; }

bool isSymbolCharacter(char character)
{
  switch(character)
  {
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
    case 'g':
    case 'h':
    case 'i':
    case 'j':
    case 'k':
    case 'l':
    case 'm':
    case 'n':
    case 'o':
    case 'p':
    case 'q':
    case 'r':
    case 's':
    case 't':
    case 'u':
    case 'v':
    case 'w':
    case 'x':
    case 'y':
    case 'z':
    case '+':
    case '-':
    case '*':
    case '/':
      return true;

    default:
      return false;
  }
}
/* end parser helpers */

/* begin parser */
ParseResult integerParser(char* source);
ParseResult sExpressionParser(char* source);
ParseResult stringParser(char* source);
ParseResult symbolParser(char* source);
ParseResult objectParser(char* source);
Object* parse(ParseResult (*parser)(char*), char* source);

ParseResult initializedParseResult(char* source)
{
  ParseResult result;
  result.succeeded = false;
  result.result = NULL;
  result.remaining = source;
  return result;
}

ParseResult integerParser(char* source)
{
  source = consumeWhitespace(source);

  ParseResult result = initializedParseResult(source);

  int32_t sign = 1;

  if(*source == '-')
  {
    sign = -1;
    source++;
  }

  if(!isDigit(*source)) return result;

  result.succeeded = true;
  result.result = constructObject(INTEGER);
  result.remaining = source; // This is necessary because we added to source to consume the -
  result.result->instance.integer = 0;

  while(isDigit(*(result.remaining)))
  {
    result.succeeded = true;
    result.result->instance.integer *= 10;
    result.result->instance.integer += toDigit(*(result.remaining));
    result.remaining++;
  }

  result.result->instance.integer *= sign;

  return result;
}

ParseResult sExpressionParserInternal(char* source, bool leadingWhitespaceRequired)
{
  ParseResult result = initializedParseResult(consumeWhitespace(source));

  if(*(result.remaining) == ')')
  {
    result.succeeded = true;
    result.result = NULL;
    result.remaining++;
    return result;
  }

  if(leadingWhitespaceRequired && source == result.remaining)
  {
    printf("Did not consume whitespace where required");
    exit(EXIT_FAILURE);
  }

  ParseResult atomParseResult = objectParser(result.remaining);

  if(!atomParseResult.succeeded)
  {
    printf("Parenthese opened but not closed");
    exit(EXIT_FAILURE);
  }

  ParseResult remainderParseResult = sExpressionParserInternal(atomParseResult.remaining, true);

  result.succeeded = true;
  result.result = constructObject(S_EXPRESSION);
  result.result->instance.sExpression.first = atomParseResult.result;
  result.result->instance.sExpression.rest = remainderParseResult.result;
  result.remaining = remainderParseResult.remaining;

  return result;
}

ParseResult sExpressionParser(char* source)
{
  ParseResult result = initializedParseResult(consumeWhitespace(source));

  if(*(result.remaining) != '(') return result;

  result.remaining++;

  return sExpressionParserInternal(result.remaining, false);
}

ParseResult stringParser(char* source)
{
  ParseResult result = initializedParseResult(consumeWhitespace(source));

  if(*(result.remaining) != '"') return result;

  result.remaining++;

  while(*(result.remaining) != '"' && *(result.remaining) != '\0')
  {
    result.remaining++;
  }

  if(*(result.remaining) == '\0')
  {
    printf("String opened and not closed.");
    exit(EXIT_FAILURE);
  }

  size_t memoryLength = result.remaining - source;

  result.succeeded = true;
  result.result = constructObject(STRING);
  result.result->instance.string.characters = memcpy(malloc(memoryLength), source + 1, memoryLength);
  result.result->instance.string.characters[memoryLength] = '\0';
  result.remaining++;

  return result;
}

ParseResult symbolParser(char* source)
{
  ParseResult result = initializedParseResult(consumeWhitespace(source));

  if(!isSymbolCharacter(*(result.remaining)))
  {
    return result;
  }

  while(isSymbolCharacter(*(result.remaining)))
  {
    result.remaining++;
  }

  size_t memoryLength = result.remaining - source + 1;

  result.succeeded = true;
  result.result = constructObject(SYMBOL);
  result.result->instance.symbol.name = memcpy(malloc(memoryLength), source, memoryLength - 1);
  result.result->instance.symbol.name[memoryLength - 1] = '\0';

  return result;
}

ParseResult objectParser(char* source)
{
#define PARSER_COUNT 4

  ParseResult (*parsers[PARSER_COUNT])(char*) =
  {
    integerParser,
    sExpressionParser,
    stringParser,
    symbolParser
  };

  ParseResult result;

  uint8_t i;
  for(i = 0; i < PARSER_COUNT; i++)
  {
    result = parsers[i](source);

    if(result.succeeded) break;
  }

  return result;
}

Object* parse(ParseResult (*parser)(char*), char* source)
{
  ParseResult result = parser(source);
  if(result.succeeded) return result.result;

  printf("Unable to parse source \"%s\".", source);
  exit(EXIT_FAILURE);
}
/* end parser */

/* begin c builtin declarations */
Object* first1(Object* sExpression);
Object* rest1(Object* sExpression);
Object* add2(Object* a, Object* b);
Object* show1(Object* object);

Object* evaluate1(Environment* environment, Object* object);
/* end c builtin declarations */

/* begin c builtin functions */
Object* first1(Object* sExpression)
{
  if(!(sExpression->tag == S_EXPRESSION))
  {
    printf("first1 only takes s-expressions");
    exit(EXIT_FAILURE);
  }

  return sExpression->instance.sExpression.first;
}

Object* rest1(Object* sExpression)
{
  if(!(sExpression->tag == S_EXPRESSION))
  {
    printf("rest1 only takes s-expressions");
    exit(EXIT_FAILURE);
  }

  return sExpression->instance.sExpression.rest;
}

Object* prepend2(Object* item, Object* sExpression)
{
  if(!(sExpression == NULL || sExpression->tag == S_EXPRESSION))
  {
    printf("prepend2 argument 2 only takes s-expressions");
    exit(EXIT_FAILURE);
  }

  Object* result = constructObject(S_EXPRESSION);
  result->instance.sExpression.first = item;
  result->instance.sExpression.rest = rereferenceObject(sExpression);
  return result;
}

Object* mathOperation(Object* a, Object* b, int32_t (*operation)(int32_t,int32_t))
{
  if(!(a->tag == INTEGER) || !(b->tag == INTEGER))
  {
    printf("Can only operate on integers");
    exit(EXIT_FAILURE);
  }

  Object* result = constructObject(INTEGER);
  result->instance.integer = operation(a->instance.integer, b->instance.integer);
  return result;
}

int32_t addOperation(int32_t a, int32_t b) { return a + b; }
Object* add2(Object* a, Object* b) { return mathOperation(a,b,addOperation); }

int32_t divideOperation(int32_t a, int32_t b) { return a / b; }
Object* divideInteger2(Object* a, Object* b) { return mathOperation(a,b,divideOperation); }

int32_t modulusOperation(int32_t a, int32_t b) { return a % b; }
Object* modulus2(Object* a, Object* b) { return mathOperation(a,b,modulusOperation); }

int32_t multiplyOperation(int32_t a, int32_t b) { return a * b; }
Object* multiply2(Object* a, Object* b) { return mathOperation(a,b,multiplyOperation); }

int32_t subtractOperation(int32_t a, int32_t b) { return a - b; }
Object* subtract2(Object* a, Object* b) { return mathOperation(a,b,subtractOperation); }

Object* show1(Object* object)
{
  Object* result = constructObject(STRING);

  String string;

  size_t memoryLength;

  if(object == NULL)
  {
    string.characters = malloc(5);
    snprintf(string.characters, 5, "null");
  }

  else switch(object->tag)
  {
    case CLOSURE:
      string.characters = malloc(10);
      snprintf(string.characters, 10, "(Closure)");
      break;

    case INTEGER:
      string.characters = itoa(object->instance.integer, malloc(11));
      break;

    case S_EXPRESSION:
      string.characters = malloc(14);
      snprintf(string.characters, 14, "(SExpression)");
      break;

    case STRING:
      memoryLength = strlen(object->instance.string.characters) + 3;

      string.characters = malloc(memoryLength);
      snprintf(string.characters, memoryLength, "\"%s\"", object->instance.string.characters);
      break;

    case SYMBOL:
      memoryLength = strlen(object->instance.symbol.name) + 2;

      string.characters = malloc(memoryLength);
      snprintf(string.characters, memoryLength, ":%s", object->instance.symbol.name);
      break;
  }

  result->instance.string = string;

  return result;
}
/* end c builtin functions */

/* begin c builtin applicatives */
Object* evaluateSExpression(Environment* environment, Object* sExpression)
{
  Object* applicative = evaluate1(environment, sExpression->instance.sExpression.first);
  return applicative->instance.closure(environment, sExpression->instance.sExpression.rest);
}

Object* evaluate1(Environment* environment, Object* object)
{
  if(object == NULL) return NULL;

  switch(object->tag)
  {
    case INTEGER:
    case STRING:
      return rereferenceObject(object);

    case S_EXPRESSION:
      return evaluateSExpression(environment, object);

    case SYMBOL:
      while(environment != NULL)
      {
        if(strcmp(object->instance.symbol.name, environment->key->instance.symbol.name) == 0)
        {
          return rereferenceObject(environment->value);
        }

        environment = environment->next;
      }

      printf("Symbol \"%s\" not defined", object->instance.symbol.name);
      exit(EXIT_FAILURE);

    default:
      printf("Unexpected object type passed to evaluate1");
      exit(EXIT_FAILURE);
  }
}
/* end c builtin applicatives */

/* begin c appliers */
size_t sExpressionLength(Object* sExpression)
{
  if(sExpression == NULL) return 0;

  if(sExpression->tag != S_EXPRESSION)
  {
    printf("argument must be an s expression");
    exit(EXIT_FAILURE);
  }

  return sExpressionLength(sExpression->instance.sExpression.rest) + 1;
}

Object* sExpressionGet(Object* sExpression, size_t index)
{
  if(index == 0) return first1(sExpression);
  return sExpressionGet(rest1(sExpression), index - 1);
}

Object* cApply2(
    Object* (*call)(Environment*,Object*,Object*),
    Environment* environment,
    Object* arguments)
{
  size_t argumentCount = sExpressionLength(arguments);

  if(argumentCount < 2)
  {
    printf("Not enough arguments");
    exit(EXIT_FAILURE);
  }

  if(argumentCount > 2)
  {
    printf("Too many arguments");
    exit(EXIT_FAILURE);
  }

  return call(environment, sExpressionGet(arguments, 0), sExpressionGet(arguments, 1));
}

Object* evaluateArguments(Environment* environment, Object* arguments)
{
  if(arguments == NULL) return NULL;

  return prepend2(
      evaluate1(environment, first1(arguments)),
      evaluateArguments(environment, rest1(arguments)));
}

Object* cApplyFunction2(
    Object* (*call)(Object*,Object*),
    Environment* environment,
    Object* arguments)
{
  Object* evaluatedArguments = evaluateArguments(environment, arguments);

  return call(
      sExpressionGet(evaluatedArguments, 0),
      sExpressionGet(evaluatedArguments, 1));
}
/* end c appliers */

/* begin lisp builtins */
Object* add(Environment* environment, Object* arguments)
{
  return cApplyFunction2(add2, environment, arguments);
}

Object* divideInteger(Environment* environment, Object* arguments)
{
  return cApplyFunction2(divideInteger2, environment, arguments);
}

Object* modulus(Environment* environment, Object* arguments)
{
  return cApplyFunction2(modulus2, environment, arguments);
}

Object* multiply(Environment* environment, Object* arguments)
{
  return cApplyFunction2(multiply2, environment, arguments);
}

Object* subtract(Environment* environment, Object* arguments)
{
  return cApplyFunction2(subtract2, environment, arguments);
}
/* end lisp builtins */

/* begin runner */
Environment* prependBuiltinClosure(
    char* name,
    Object* (*call)(Environment*,Object*),
    Environment* next)
{
  size_t nameLength = strlen(name);

  Object* key = constructObject(SYMBOL);
  key->instance.symbol.name = malloc(nameLength + 1);
  snprintf(key->instance.symbol.name, nameLength + 1, name);

  Object* value = constructObject(CLOSURE);
  value->instance.closure = call;

  return constructEnvironment(key, value, next);
}

void repl()
{
  Environment* environment = NULL;
  environment = prependBuiltinClosure("+", add, environment);
  environment = prependBuiltinClosure("//", divideInteger, environment);
  environment = prependBuiltinClosure("mod", modulus, environment);
  environment = prependBuiltinClosure("*", multiply, environment);
  environment = prependBuiltinClosure("-", subtract, environment);

  char* input, shell_prompt[100];

  // Configure readline to auto-complete paths when the tab key is hit.
  rl_bind_key('\t', rl_complete);

  for(;;)
  {
    // Create prompt string from user name and current working directory.
    snprintf(shell_prompt, sizeof(shell_prompt), ">>> ");

    // Display prompt and read input (NB: input must be freed after use)...
    input = readline(shell_prompt);

    // Check for EOF.
    if (!input)
    {
      free(input);
      break;
    }

    // Add input to history.
    add_history(input);

    /* begin handling input */
    Object* expression = parse(objectParser, input);
    Object* result = evaluate1(environment, expression);
    Object* display = show1(result);

    printf("%s\n", display->instance.string.characters);

    dereferenceObject(result);
    dereferenceObject(expression);
    dereferenceObject(display);
    /* end handling input */

    free(input);
  }
}

int main()
{
  repl();
  return EXIT_SUCCESS;
}
/* end runner */
