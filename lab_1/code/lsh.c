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
 * Using assert statements is a good way to catch errors early and make
 * debugging easier. Think of them as mini self-checks that ensure your program
 * behaves as expected. By setting up these guardrails, you're creating a more
 * robust and maintainable solution. So go ahead, sprinkle some asserts in your
 * code; they're your friends in disguise!
 *
 * All the best!
 */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The <unistd.h> header is your gateway to the OS's process management
// facilities.
#include <unistd.h>

#include "parse.h"

#include <fcntl.h>
#include <sys/wait.h>
#include <sysexits.h>

static void print_cmd(Command *cmd);
static void print_pgm(Pgm *p);
void stripwhite(char *);

void run_command(Pgm *pgm);

int main(void)
{
  for (;;)
  {
    char *line;
    line = readline("> ");

    // CTRL+D: exit
    if (line == NULL)
    {
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
        if (strcmp(cmd.pgm->pgmlist[0], "exit") == 0)
        {
          free(line);
          printf("Exiting...\n");
          exit(0);
        }
        else if (strcmp(cmd.pgm->pgmlist[0], "cd") == 0)
        {
          chdir(cmd.pgm->pgmlist[1]);
          continue;
        }

        pid_t pid = fork();
        if (pid < 0)
        {
          printf("error accured!");
          return -1;
        }
        else if (pid == 0)
        {

          // We can pass pgmlist as a vector into execvp, instead of manually
          // parsing list elements into execlp

          run_command(cmd.pgm);

          /*int result = execvp(cmd.pgm->pgmlist[0], cmd.pgm->pgmlist);

          printf("child process %d - %s \n", result, strerror(errno));
          write(pipefds[1], &errno, sizeof(int)); */
          return 0;
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

void run_command(Pgm *pgm)
{
  int num_stages = 0;

  for (Pgm *p = pgm; p != NULL; p = p->next)
  {
    num_stages++;
  }

  Pgm *stages[num_stages];

  // Reverse the order of the stages to match the original command order (aestetic)
  int i = num_stages - 1;
  for (Pgm *p = pgm; p != NULL; p = p->next, i--)
  {
    stages[i] = p;
  }

  pid_t pids[num_stages];
  int prev_fd = -1; // The read end of the previous pipe

  for (int i = 0; i < num_stages; i++)
  {
    int pipefd[2];
    int write_fd = -1; // The write end of the current pipe

    // If this is not the last stage, create a pipe for the next stage
    if (i < num_stages - 1)
    {
      pipe(pipefd);
      write_fd = pipefd[1];
    }

    pid_t pid = fork(); // New process for each stage

    if (pid < 0)
    {
      perror("fork");
      _exit(EXIT_FAILURE);
    }
    else if (pid == 0) // Child
    {
      // Open the pipe ends for the current stage
      if (prev_fd != -1) // First stage has no pipe end to read from
      {
        dup2(prev_fd, STDIN_FILENO);
      }
      if (write_fd != -1) // Last stage has no pipe end to write to
      {
        dup2(write_fd, STDOUT_FILENO);
      }

      // Dup2 will not consume the file descriptors, so we need to close them in the child process
      if (prev_fd != -1)
      {
        close(prev_fd);
      }
      if (write_fd != -1)
      {
        close(write_fd);
      }
      if (i < num_stages - 1)
      {
        close(pipefd[0]); // this stage doesn't read from the pipe, so close the read end
      }

      execvp(stages[i]->pgmlist[0], stages[i]->pgmlist);
      perror("execvp");
      _exit(EXIT_FAILURE);
    }

    pids[i] = pid;

    if (prev_fd != -1)
    {
      close(prev_fd);
    }
    if (write_fd != -1)
    {
      close(write_fd);
    }

    prev_fd = (i < num_stages - 1) ? pipefd[0] : -1;
  }

  // Wait for children
  for (int i = 0; i < num_stages; i++)
  {
    waitpid(pids[i], NULL, 0);
  }
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
 * Helper function, no need to change. It may be useful to study for
 * inspiration.
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