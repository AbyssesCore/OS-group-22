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
#include <signal.h>

static void print_cmd(Command *cmd);
static void print_pgm(Pgm *p);
void stripwhite(char *);

void run_command(Pgm *pgm, int background, char *rstdin, char *rstdout);

int main(void)
{
  signal(SIGINT, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);

  pid_t shell_pgid = getpid();
  setpgid(shell_pgid, shell_pgid);
  tcsetpgrp(STDIN_FILENO, shell_pgid);

  for (;;)
  {
    while (waitpid(-1, NULL, WNOHANG) > 0); // Reap any zombie processes

    char *line;
    line = readline("> ");

    // CTRL+D: exit
    if (line == NULL)
    {
      printf("CTRL+D: EXITING...\n");
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
          printf("exit: EXITING...\n");
          exit(0);
        }
        else if (strcmp(cmd.pgm->pgmlist[0], "cd") == 0)
        {
          chdir(cmd.pgm->pgmlist[1]);
          continue;
        }

        run_command(cmd.pgm, cmd.background, cmd.rstdin, cmd.rstdout);

        if (!cmd.background)
          tcsetpgrp(STDIN_FILENO, shell_pgid);
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

void run_command(Pgm *pgm, int background, char *rstdin, char *rstdout)
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

  pid_t job_pgid = 0;

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

    if (pid == 0)
    {
      if (rstdout != NULL && i >= num_stages - 1)
      {
        int fd = open(rstdout, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0)
        {
          perror("open");
          _exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
      }

      if (rstdin != NULL && i == 0)
      {
        int fd = open(rstdin, O_RDONLY);
        if (fd < 0)
        {
          perror("open");
          _exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
      }


      if (i == 0)
      {
        setpgid(0, 0);
      }
      else
      {
        setpgid(0, job_pgid);
      }
    }
    else
    {
      if (i == 0)
        job_pgid = pid;
      setpgid(pid, job_pgid);
    }

    if (background)
    {
      signal(SIGINT, SIG_IGN);
      signal(SIGTTOU, SIG_IGN);
      signal(SIGTTIN, SIG_IGN);
    }
    else if (pid == 0)
    {
      signal(SIGINT, SIG_DFL);
      signal(SIGTTOU, SIG_DFL);
      signal(SIGTTIN, SIG_DFL);
    }

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

  if (!background)
    tcsetpgrp(STDIN_FILENO, job_pgid);

  if (background == 0)
  {
    // Wait for children
    for (int i = 0; i < num_stages; i++)
    {
      waitpid(pids[i], NULL, 0);
    }
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