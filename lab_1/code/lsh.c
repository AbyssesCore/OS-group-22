/*
 * Main source file for the lsh shell program.
 *
 * You are free to add functions to this file.
 * If you want to add functions in separate files,
 * you will need to modify CMakeLists.txt to compile
 * your additional files.
 *
 * Add appropriate comments to make your code
 * easier for us to grade.
 *
 * Using assert statements is a good way to catch errors early and make debugging easier.
 * Think of them as mini self-checks that ensure your program behaves as expected.
 * By setting up these guardrails, you're creating a more robust and maintainable solution.
 * So go ahead, sprinkle some asserts in your code; they're your friends in disguise!
 *
 * All the best!
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <errno.h>

// The <unistd.h> header is your gateway to the OS's process management facilities.
#include <unistd.h>

#include "parse.h"

#include <sysexits.h>
#include <sys/wait.h>
#include <fcntl.h>

static void print_cmd(Command *cmd);
static void print_pgm(Pgm *p);
void stripwhite(char *);

int main(void)
{
	int pipefds[2];
	int count, err;

	if (pipe(pipefds)) {
		perror("pipe");
		return EX_OSERR;
    	}
    	if (fcntl(pipefds[1], F_SETFD, fcntl(pipefds[1], F_GETFD) | FD_CLOEXEC)) {
		perror("fcntl");
		return EX_OSERR;
    	}	
 	
	for (;;)
  	{
	    char *line;
	    line = readline("> ");

		// CTRL+D: exit
		if(line == NULL) {
			printf("Exiting...\n");
			exit(0);
		}

	    // Remove leading and trailing whitespace from the line
	    stripwhite(line);

	    // If the stripped line is not blank
	    if (*line)
	    {
	      add_history(line);

	      Command cmd;
	      if (parse(line, &cmd) == 1)
	      {
		// Print the parsed command
		print_cmd(&cmd);

		// Handle the "exit" command (terminate the shell)
		if(strcmp(cmd.pgm->pgmlist[0], "exit") == 0) {
			free(line);
			printf("Exiting...\n");
			exit(0);
		}

		pid_t pid = fork();
		if (pid < 0) {
			printf("error accured!");
			return -1;
		}
		else if (pid == 0) {
			close(pipefds[0]);

			// We can pass pgmlist as a vector into execvp, instead of manually parsing list elements into execlp
			int result = execvp(cmd.pgm->pgmlist[0], cmd.pgm->pgmlist);
			
			printf("child process %d - %s \n", result, strerror(errno));
			write(pipefds[1], &errno, sizeof(int));
			return 0;
		}
		else {
			close(pipefds[1]);
			while ((count = read(pipefds[0], &err, sizeof(errno))) == -1)
			    if (errno != EAGAIN && errno != EINTR) break;
			if (count) {
			    fprintf(stderr, "child's execvp: %s\n", strerror(err));
			}
			close(pipefds[0]);
		}
		
	      }
	      else
	      {
		printf("Parse ERROR\n");
	      }
	    }

	    // Free the input buffer
	    free(line);
	  }

	  return 0;
	}

	/*
	 * Print a Command structure as returned by parse on stdout.
	 *
	 * Helper function, no need to change. Might be useful to study as inspiration.
	 */
	static void print_cmd(Command *cmd_list)
	{
	  printf("------------------------------\n");
	  printf("Parse OK\n");
	  printf("stdin:      %s\n", cmd_list->rstdin ? cmd_list->rstdin : "<none>");
	  printf("stdout:     %s\n", cmd_list->rstdout ? cmd_list->rstdout : "<none>");
	  printf("background: %s\n", cmd_list->background ? "true" : "false");
	  printf("Pgms:\n");
	  print_pgm(cmd_list->pgm);
	  printf("------------------------------\n");
	}

	/* Print a linked list of Pgm structures.
	 *
	 * Helper function, no need to change. It may be useful to study for inspiration.
	 */
static void print_pgm(Pgm *p)
{
  if (p == NULL)
  {
    return;
  }
  else
  {
    char **pl = p->pgmlist;

    /* The list is stored in reverse order, so print
     * it in reverse to restore the original order.
     */
    print_pgm(p->next);
    printf("            * [ ");
    while (*pl)
    {
      printf("%s ", *pl++);
    }
    printf("]\n");
  }
}


/* Strip whitespace from the start and end of a string.
 *
 * Helper function, no need to change.
 */
void stripwhite(char *string)
{
  size_t i = 0;

  while (isspace(string[i]))
  {
    i++;
  }

  if (i)
  {
    memmove(string, string + i, strlen(string + i) + 1);
  }

  i = strlen(string) - 1;
  while (i > 0 && isspace(string[i]))
  {
    i--;
  }

  string[++i] = '\0';
}

#define ResizeBy 8

int breakStringToStringArray(char* string, char breakBy, char ***arrayPointer) {
	size_t strLen = strlen(string);
	
	char* newString = malloc(strLen);
	
	*arrayPointer = malloc(sizeof(char**) * ResizeBy);

	int progress = 0;
	
	(*arrayPointer)[progress++] = newString;	
	
	memcpy(newString, string, strLen);
	
	for (char *ptr = newString; *ptr != '\0'; ptr++) {
		if (*ptr == breakBy) {
			*ptr = '\0';
			
			if (progress % ResizeBy == 0)
				*arrayPointer = realloc(*arrayPointer, sizeof(char**) * (ResizeBy + progress));
			
			(*arrayPointer)[progress++] = ptr + 1;	
		}

	}

	return progress;

}
