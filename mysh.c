#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <wait.h>

#define CHAR_BUF_SIZE 512
#define NUM_OF_TOKENS 32

char name[CHAR_BUF_SIZE] = "mysh";
char *command[NUM_OF_TOKENS];
int command_size = 0;

// <>&
char *redirection[3];

short int exit_status = 0;

int debug = 0;

// writes to char *command
void readline(char *line)
{
	redirection[0][0] = '\0';
	redirection[1][0] = '\0';
	redirection[2][0] = '\0';
	for (int i = 0; i < command_size; i++) {
		command[i][0] = '\0';
	}
	command_size = 0;
	int line_size = 0;
	char c = line[line_size++]; // equivalent to getchar() on char[]
	while (isspace(c)) c = line[line_size++];
	if (c == '#') {
		while (c != '\n') c = line[line_size++];
		command[0][0] = '\0';
		return;
	}
	if (c == '\n') {
		command[0][0] = '\0';
		return;
	}
	if (c == EOF) {
		exit(0);
	}

	int buf_size = 0;

	while (c != '\n' && c != '\0') {
		if (c == '"') {
			c = line[line_size++];
			while (c != '"') {
				if (buf_size > CHAR_BUF_SIZE) {
					printf("CHAR_BUF_SIZE limit (%d) exceeded at line %d\n", CHAR_BUF_SIZE, __LINE__);
					exit(1);
				}
				command[command_size][buf_size] = c;
				buf_size++;
				command[command_size][buf_size] = '\0';
				c = line[line_size++];
			}
			c = line[line_size++];
		} else if (isspace(c)) {
			command_size++;
			buf_size = 0;
			while (isspace(c)) {
				if (c == '\n') break;
				c = line[line_size++];
			}
		} else {
			command[command_size][buf_size] = c;
			buf_size++;
			command[command_size][buf_size] = '\0';
			c = line[line_size++];
		}
	}
	command_size++;

	// <vhod >izhod &
	if (command[command_size - 1][0] == '&') {
		strcpy(redirection[2], command[command_size - 1]);
		command_size--;
	}
	if (command[command_size - 1][0] == '>') {
		strcpy(redirection[1], command[command_size - 1] + 1);
		command_size--;
	}
	if (command[command_size - 1][0] == '<') {
		strcpy(redirection[0], command[command_size - 1] + 1);
		command_size--;
	}
}

void builtin_name(int argc, char *argv[])
{
	if (argc < 2) {
		printf("%s\n", name);
	} else {
		strncpy(name, argv[1], CHAR_BUF_SIZE);
	}
	exit_status = 0;
}

void builtin_help()
{
	puts("      name - Print or change shell name");
	puts("      help - Print short help");
	puts("     debug - Toggle debug mode");
	puts("    status - Print last command status");
	puts("      exit - Exit from shell");
	puts("     print - Print arguments");
	puts("      echo - Print arguments and newline");
	puts("       pid - Print PID");
	puts("      ppid - Print PPID");
	puts("       dir - Change directory");
	puts("  dirwhere - Print current working directory");
	exit_status = 0;
}

void builtin_status()
{
	// TODO in all functions add exit_status
	printf("%d\n", exit_status);
	exit_status = 0;
}

void builtin_exit(int argc, char *argv[])
{
	if (argc < 2) {
		exit(0);
	}
	exit(atoi(argv[1]));
}

void builtin_print(int argc, char *argv[])
{
	printf("%s", argv[1]);
	for (int i = 2; i < argc; i++) {
		printf(" %s", argv[i]);
	}
	exit_status = 0;
}

void builtin_echo(int argc, char *argv[])
{
	builtin_print(argc, argv);
	puts("");
	exit_status = 0;
}

void builtin_debug()
{
	debug ^= 1;
	if (debug) puts("Debug mode enabled");
	else puts("Debug mode disabled");
	exit_status = 0;
}

void builtin_pid()
{
	printf("%d\n", getpid());
	exit_status = 0;
}

void builtin_ppid()
{
	printf("%d\n", getppid());
	exit_status = 0;
}

void builtin_dirchange()
{
	if (command_size < 2) {
		// goto "/"
		command[1][0] = '/';
		command[1][1] = '\0';
		command_size = 2;
	}

	errno = 0;
	chdir(command[1]);
	int serrno = errno;
	if (errno) perror("dirchange");
	exit_status = serrno;
}

void builtin_dirwhere()
{
	char *buffer = (char *)malloc(CHAR_BUF_SIZE);
	getcwd(buffer, CHAR_BUF_SIZE);
	printf("%s\n", buffer);
	free(buffer);
	exit_status = 0;
}

void builtin_dirmake(int argc, char *argv[])
{
	if (argc < 2) {
		puts("No name was specified");
		exit_status = 1;
		return;
	}
	errno = 0;
	mkdir(argv[1], 0777);
	int serrno = errno;
	if (errno) perror("dirmake");
	exit_status = serrno;
}

void builtin_dirremove(int argc, char *argv[])
{
	if (argc < 2) {
		puts("No name was specified");
		exit_status = 1;
		return;
	}
	errno = 0;
	rmdir(argv[1]);
	int serrno = errno;
	if (errno) perror("dirremove");
	exit_status = serrno;
}

void builtin_dirlist()
{
	if (command_size < 2) {
		// goto "."
		command[1][0] = '.';
		command[1][1] = '\0';
		command_size = 2;
	}

	errno = 0;
	DIR *dir = opendir(command[1]);
	int serrno = errno;
	if (errno) {
		perror("opendir");
		exit_status = serrno;
		return;
	}
	for (struct dirent *tmp = readdir(dir); tmp != NULL; tmp = readdir(dir)) {
		printf("%s  ", tmp->d_name);
	}
	puts("");
	errno = 0;
	closedir(dir);
	serrno = errno;
	if (errno) perror("closedir");
	exit_status = serrno;
}

void builtin_linkhard(int argc, char *argv[])
{
	if (argc < 3) {
		puts("Too few arguments");
		return;
	}
	if (link(argv[1], argv[2])) {
		int serrno = errno;
		perror("linkhard");
		exit_status = serrno;
	} else {
		exit_status = 0;
	}
}
void builtin_linksoft(int argc, char *argv[])
{
	if (argc < 3) {
		puts("Too few arguments");
		return;
	}
	if (symlink(argv[1], argv[2])) {
		int serrno = errno;
		perror("linksoft");
		exit_status = serrno;
	} else {
		exit_status = 0;
	}
}

void builtin_linkread(int argc, char *argv[])
{
	char *buffer = (char *)malloc(CHAR_BUF_SIZE);
	errno = 0;
	buffer[readlink(argv[1], buffer, CHAR_BUF_SIZE - 1)] = '\0';
	if (errno) {
		int serrno = errno;
		perror("linkread");
		exit_status = serrno;
	} else {
		printf("%s\n", buffer);
		exit_status = 0;
	}
	free(buffer);
}

void builtin_linklist(int argc, char *argv[])
{
	// find files with matching inode number
	struct stat statbuf;
	stat(command[1], &statbuf);
	int inode_num = statbuf.st_ino;
	errno = 0;
	DIR *dir = opendir(".");
	int serrno = errno;
	if (errno) {
		perror("opendir");
		exit_status = serrno;
		return;
	}
	for (struct dirent *tmp = readdir(dir); tmp != NULL; tmp = readdir(dir)) {
		char path[300];
		path[0] = '.';
		path[1] = '/';
		strcpy(path + 2, tmp->d_name);
		lstat(path, &statbuf);
		if (inode_num == statbuf.st_ino) {
			printf("%s  ", path + 2);
		}
	}
	puts("");
	errno = 0;
	closedir(dir);
	serrno = errno;
	if (errno) perror("closedir");
	exit_status = serrno;
}

void builtin_unlink()
{
	errno = 0;
	unlink(command[1]);
	if (errno) perror("unlink");
	exit_status = errno;
}

void builtin_rename()
{
	errno = 0;
	rename(command[1], command[2]);
	if (errno) perror("unlink");
	exit_status = errno;
}

void builtin_cpcat(int argc, char *argv[])
{
	int in = -1;
	int out = -1;

	if (argc <= 1) {
		in = dup(0);
		out = dup(1);
	} else if (argc == 2) {
		if (argv[1][0] == '-')
			in = dup(0);
		else
			in = open(argv[1], O_RDONLY);
		out = dup(1);
	} else {
		if (argv[1][0] == '-')
			in = dup(0);
		else
			in = open(argv[1], O_RDONLY);
		if (argv[2][0] == '-')
			out = dup(1);
		else
			out = open(argv[2], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	}

	// check syscall dup / open return value
	if (in == -1) {
		perror(argv[1]);
		exit_status = errno;
		return;
	}
	if (out == -1) {
		perror(argv[2]);
		exit_status = errno;
		return;
	}

	char chr;
	int read_ret = 1;
	int write_ret = 1;

	while (read_ret) {
		// checking all return values is ugly
		read_ret = read(in, &chr, 1);
		if (read_ret == -1) {
			perror(argv[0]);
			exit_status = errno;
			return;
		}
		else if (read_ret == 0) {
			break;
		}
		write_ret = write(out, &chr, 1);
		if (write_ret == -1) {
			perror(argv[0]);
			exit_status = errno;
			return;
		}
	}

	// only close file (and not stdin)
	if (in != 0) {
		if (close(in) == -1) {
			perror(argv[1]);
			exit_status = errno;
			return;
		}
	}
	// only close file (and not stdout)
	if (out != 1) {
		if (close(out) == -1) {
			perror(argv[2]);
			exit_status = errno;
			return;
		}
	}

	exit_status = 0;
}

void execute();

void swap(int left_pipe[], int right_pipe[])
{
	int tmp[2];
	tmp[0] = right_pipe[0];
	tmp[1] = right_pipe[1];
	right_pipe[0] = left_pipe[0];
	right_pipe[1] = left_pipe[1];
	left_pipe[0] = tmp[0];
	left_pipe[1] = tmp[1];
}

void builtin_pipes()
{
	char **todo = (char **)malloc((command_size - 1) * sizeof(char *));
	int len = command_size - 1;
	int current_command = 0;
	for (int i = 0; i < len; i++) {
		todo[i] = (char *)malloc(CHAR_BUF_SIZE);
		strcpy(todo[i], command[i + 1]);
	}

////////////////////////////////////////////////////////////////////////////////
	int left_pipe[2];
	int right_pipe[2];
	int pid;
	int base_stdin = dup(STDIN_FILENO);
	int base_stdout = dup(STDOUT_FILENO);

	// Entry
	readline(todo[current_command++]);
	pipe(right_pipe);
	pid = fork();
	if (pid == 0) {
		dup2(right_pipe[1], STDOUT_FILENO);
		close(right_pipe[1]);
		close(right_pipe[0]);
		execute();
		exit(0);
	} else {
		waitpid(pid, NULL, 0);
	}

	if (len >= 3) {

		//// Loop
		for (int i = 1; i < len - 1; i++) {
			pipe(left_pipe);
			readline(todo[current_command++]);
			swap(left_pipe, right_pipe);
			pid = fork();
			if (pid == 0) {
				dup2(left_pipe[0], STDIN_FILENO);
				dup2(right_pipe[1], STDOUT_FILENO);
				close(left_pipe[0]);
				close(left_pipe[1]);
				close(right_pipe[0]);
				close(right_pipe[1]);
				execute();
				exit(0);
			}
			close(left_pipe[0]);
			close(left_pipe[1]);
		}

	}

	// Exit
	readline(todo[current_command++]);
	swap(left_pipe, right_pipe);
	close(left_pipe[1]);
	pid = fork();
	if (pid == 0) {
		dup2(left_pipe[0], STDIN_FILENO);
		close(left_pipe[0]);
		execute();
		exit(0);
	} else {
		waitpid(pid, NULL, 0);
	}
	close(left_pipe[0]);
	dup2(base_stdin, STDIN_FILENO);
	dup2(base_stdout, STDOUT_FILENO);
////////////////////////////////////////////////////////////////////////////////

	// cleanup
	for (int i = 0; i < command_size - 1; i++) {
		free(todo[i]);
	}
	free(todo);
}

void execute()
{
	if (!strlen(command[0])) return;
	// built-in commands [base]
	if (!strcmp(command[0], "name")) builtin_name(command_size, command);
	else if (!strcmp(command[0], "help")) builtin_help();
	else if (!strcmp(command[0], "status")) builtin_status();
	else if (!strcmp(command[0], "exit")) builtin_exit(command_size, command);
	else if (!strcmp(command[0], "print")) builtin_print(command_size, command);
	else if (!strcmp(command[0], "echo")) builtin_echo(command_size, command);
	else if (!strcmp(command[0], "debug")) builtin_debug();
	else if (!strcmp(command[0], "pid")) builtin_pid();
	else if (!strcmp(command[0], "ppid")) builtin_ppid();
	// built-in commands [dir]
	else if (!strcmp(command[0], "dirchange")) builtin_dirchange();
	else if (!strcmp(command[0], "dirwhere")) builtin_dirwhere();
	else if (!strcmp(command[0], "dirmake")) builtin_dirmake(command_size, command);
	else if (!strcmp(command[0], "dirremove")) builtin_dirremove(command_size, command);
	else if (!strcmp(command[0], "dirlist")) builtin_dirlist(command_size, command);
	// built-in commands [dir other]
	else if (!strcmp(command[0], "linkhard")) builtin_linkhard(command_size, command);
	else if (!strcmp(command[0], "linksoft")) builtin_linksoft(command_size, command);
	else if (!strcmp(command[0], "linkread")) builtin_linkread(command_size, command);
	else if (!strcmp(command[0], "linklist")) builtin_linklist(command_size, command);
	else if (!strcmp(command[0], "unlink")) builtin_unlink();
	else if (!strcmp(command[0], "rename")) builtin_rename();
	else if (!strcmp(command[0], "cpcat")) builtin_cpcat(command_size, command);
	else if (!strcmp(command[0], "pipes")) builtin_pipes();

	// fork && exec
	else {
		char *argv[command_size + 1];
		for (int i = 0; i < command_size; i++) {
			argv[i] = command[i];
		}
		argv[command_size] = NULL;

		int pid = fork();
		if (pid == 0) {
			execvp(argv[0], argv);
			exit(0);
		} else {
			waitpid(pid, NULL, 0);
		}
	}
}

static void sigchld_handler(int sig)
{
    pid_t pid;
    int status;

    while((pid = waitpid(-1, &status, WNOHANG)) > 0) {};
}


void redirect_and_execute()
{
	// debug print
	if (debug) {
		printf("command: ");
		for (int i = 0; i < command_size; i++) {
			printf("%s ", command[i]);
		}
		puts("");

		printf("input: %s\n", redirection[0]);
		printf("output: %s\n", redirection[1]);
		if (redirection[2][0] != '&') {
			printf("Foreground operation\n");
		} else {
			printf("Background operation\n");
		}
	}

	int base_stdin = dup(STDIN_FILENO);
	int base_stdout = dup(STDOUT_FILENO);
	int fdin = base_stdin;
	int fdout = base_stdout;
	// redirection
	if (redirection[0][0] != '\0') {
		// <vhod
		fdin = open(redirection[0], O_RDONLY);
		fflush(stdin);
		dup2(fdin, STDIN_FILENO);
	}
	if (redirection[1][0] != '\0') {
		// >izhod
		fdout = open(redirection[1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
		fflush(stdout);
		dup2(fdout, STDOUT_FILENO);
	}


	// background
	if (redirection[2][0] != '&') {
		execute();
	} else {
		int pid = fork();
		if (pid == 0) {
			execute();
			exit(0);
		} else {
			// wait(NULL); -> create SIGCHLD handler
		}
	}

	// reset state
	if (redirection[0][0] != '\0') {
		dup2(base_stdin, STDIN_FILENO);
		close(fdin);
	}
	if (redirection[1][0] != '\0') {
		dup2(base_stdout, STDOUT_FILENO);
		close(fdout);
	}
	close(base_stdin);
	close(base_stdout);
}

void interactive_mode(int argc, char *argv[])
{
	while (1) {
		printf("%s> ", name);
		char line[CHAR_BUF_SIZE];
		if (fgets(line, CHAR_BUF_SIZE-1, stdin) == NULL) return;
		readline(line);
		redirect_and_execute();
	}
}

void scripting_mode(int argc, char *argv[])
{
	while (1) {
		char line[CHAR_BUF_SIZE];
		if (fgets(line, CHAR_BUF_SIZE-1, stdin) == NULL) return;
		readline(line);
		redirect_and_execute();
	}
}

void setup() {
	setvbuf(stdin, 0, _IONBF, 0);
	setvbuf(stdout, 0, _IONBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);
}

int main(int argc, char *argv[])
{
	setup();
	signal(SIGCHLD, sigchld_handler);
	for (int i = 0; i < NUM_OF_TOKENS; i++) {
		command[i] = (char *)malloc(CHAR_BUF_SIZE);
	}
	for (int i = 0; i < 3; i++) {
		redirection[i] = (char *)malloc(CHAR_BUF_SIZE);
	}
	if (isatty(0)) {
		interactive_mode(argc, argv);
	} else {
		scripting_mode(argc, argv);
	}

	for (int i = 0; i < 3; i++) {
		free(redirection[i]);
		redirection[i] = NULL;
	}
	for (int i = 0; i < NUM_OF_TOKENS; i++) {
		free(command[i]);
		command[i] = NULL;
	}
	return 0;
}
