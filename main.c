#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

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
        if (!input) break;

        // Add input to history.
        add_history(input);

	printf("%s\n",input);

        // Free input.
        free(input);
    }
}

int main(int argc, char** argv)
{
    repl();
    return EXIT_SUCCESS;
}
