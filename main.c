#include <readline/history.h>
#include <readline/readline.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* begin types */
enum Tag;
union Instance;
struct Object;

typedef enum Tag Tag;
typedef union Instance Instance;
typedef struct Object Object;

enum Tag
{
    INTEGER
};

union Instance
{
    int32_t integer;
};

struct Object
{
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

Object* parse(ParseResult (*parser)(char*), char* source)
{
    ParseResult result = parser(source);
    if(result.succeeded) return result.result;
    return NULL;
}
/* end parser */

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

        Object* result = parse(integerParser, input);

        if(result == NULL) printf("null\n");
	else printf("%i\n", result->instance.integer);

	free(result);

        // Free input.
        free(input);
    }
}

int main()
{
    repl();
    return EXIT_SUCCESS;
}
/* end runner */
