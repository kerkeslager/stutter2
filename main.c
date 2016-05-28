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
struct String;
struct Symbol;

enum Tag;
union Instance;
struct Object;

typedef struct String String;
typedef struct Symbol Symbol;

typedef enum Tag Tag;
typedef union Instance Instance;
typedef struct Object Object;

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
  INTEGER,
  STRING,
  SYMBOL
};

union Instance
{
  int32_t integer;
  String string;
  Symbol symbol;
};

struct Object
{
  size_t referenceCount;
  Tag tag;
  Instance instance;
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

/* begin dereferencers */
Object* rereferenceObject(Object* self)
{
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
    case INTEGER:
      free(self);
      break;

    case STRING:
      free(self->instance.string.characters);
      free(self);
      break;

    case SYMBOL:
      break;

    default:
      printf("Dereference object called with unexpected type");
      exit(EXIT_FAILURE);
  }
}
/* end dereferencers */

/* begin parser */
ParseResult integerParser(char* source);
Object* parse(ParseResult (*parser)(char*), char* source);

bool isDigit(char character) { return '0' <= character && character <= '9'; }
int32_t toDigit(char character) { return character - '0'; }

ParseResult integerParser(char* source)
{
  ParseResult result;
  result.succeeded = false;
  result.result = NULL;
  result.remaining = source;

  if(!isDigit(*source)) return result;

  result.succeeded = true;
  result.result = malloc(sizeof(Object));
  result.result->referenceCount = 1;
  result.result->tag = INTEGER;
  result.result->instance.integer = 0;

  while(isDigit(*(result.remaining)))
  {
    result.succeeded = true;
    result.result->instance.integer *= 10;
    result.result->instance.integer += toDigit(*(result.remaining));
    result.remaining++;
  }

  return result;
}

ParseResult stringParser(char* source)
{
  ParseResult result;
  result.succeeded = false;
  result.result = NULL;
  result.remaining = source;

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
  result.result = malloc(sizeof(Object));
  result.result->referenceCount = 1;
  result.result->tag = STRING;
  result.result->instance.string.characters = memcpy(malloc(memoryLength), source + 1, memoryLength);
  result.result->instance.string.characters[memoryLength - 1] = '\0';
  result.remaining++;

  return result;
}

bool isSymbolCharacter(char character)
{
  return 'a' <= character && character <= 'z';
}

ParseResult symbolParser(char* source)
{
  ParseResult result;
  result.succeeded = false;
  result.result = NULL;
  result.remaining = source;

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
  result.result = malloc(sizeof(Object));
  result.result->referenceCount = 1;
  result.result->tag = SYMBOL;
  result.result->instance.symbol.name = memcpy(malloc(memoryLength), source, memoryLength - 1);
  result.result->instance.symbol.name[memoryLength - 1] = '\0';

  return result;
}

ParseResult objectParser(char* source)
{
#define PARSER_COUNT 3

  ParseResult (*parsers[PARSER_COUNT])(char*) =
  {
    integerParser,
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

/* begin builtins */
Object* evaluate1(Object* object)
{
  if(object == NULL) return NULL;

  switch(object->tag)
  {
    case INTEGER:
    case STRING:
      return rereferenceObject(object);

    case SYMBOL:
      return NULL;

    default:
      printf("Unexpected object type passed to evaluate1");
      exit(EXIT_FAILURE);
  }
}

Object* show1(Object* object)
{
  Object* result = malloc(sizeof(Object));
  result->tag = STRING;

  String string;

  size_t memoryLength;

  if(object == NULL)
  {
    string.characters = malloc(5);
    snprintf(string.characters, 5, "null");
  }

  else switch(object->tag)
  {
    case INTEGER:
      string.characters = itoa(object->instance.integer, malloc(11));
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
/* end builtins */

/* begin runner */
void repl()
{
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
    Object* result = evaluate1(expression);
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
