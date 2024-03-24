#include <fcntl.h>
#include <pwd.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BOLD "\033[1m" // 加粗

// 文本颜色
#define BLACK "\033[30m"   // 黑色
#define RED "\033[31m"     // 红色
#define GREEN "\033[32m"   // 绿色
#define YELLOW "\033[33m"  // 黄色
#define BLUE "\033[34m"    // 蓝色
#define MAGENTA "\033[35m" // 洋红色
#define CYAN "\033[36m"    // 青色
#define WHITE "\033[37m"   // 白色

#define LENGTH 2048

char last_dir[LENGTH] = "";                     // 存储上一个目录
char* generate_prompt();                        // 生成提示符
void do_pipe(int left, int right, char** args); // 处理管道命令
void my_execute(char** args, int cnt);          // 执行命令
void signal_handler(int signum)
{
    printf("\n");
    fflush(stdout);
}

// 生成提示符
char* generate_prompt()
{
    // 获取当前时间
    time_t current_time;
    struct tm* timeinfo;
    char time_string[12]; // HH:MM:SS 格式
    time(&current_time);
    timeinfo = localtime(&current_time);
    strftime(time_string, sizeof(time_string), "[%H:%M:%S]", timeinfo);

    // 获取当前用户名和目录
    struct passwd* pwd = getpwuid(getuid());
    char* username = pwd->pw_name;
    char cwd[LENGTH];
    if (getcwd(cwd, LENGTH) == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    // 将 "/home/linyu" 替换为 "~"
    char* home_dir = getenv("HOME");
    if (home_dir != NULL && strstr(cwd, home_dir) == cwd) {
        char* relative_path = cwd + strlen(home_dir);
        char* temp_prompt = (char*)malloc(LENGTH * sizeof(char));
        if (temp_prompt == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        sprintf(
            temp_prompt,
            BLUE BOLD "# %s\033[0m" CYAN " @ " GREEN "%s\033[0m" CYAN " in\033[0m " YELLOW
                      "~%s\033[0m" CYAN " %s\033[0m\n" RED BOLD "$\033[0m " WHITE,
            username,
            username,
            relative_path,
            time_string);
        return temp_prompt;
    }

    // 构造提示符
    char* prompt = (char*)malloc(LENGTH * sizeof(char));
    if (prompt == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    sprintf(
        prompt,
        BLUE BOLD
        "# %s\033[0m"
        " " CYAN
        "@\033[0m"
        " " GREEN
        "%s\033[0m"
        " " CYAN
        "in\033[0m"
        " " YELLOW
        "%s\033[0m"
        " " CYAN "%s\033[0m\n" RED BOLD
        "$\033[0m"
        " " WHITE,
        username,
        username,
        cwd,
        time_string);

    return prompt;
}

int main()
{
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGTSTP, signal_handler);
    while (1) {
        if (feof(stdin)) {
            printf("\n");
            break;
        }
        struct passwd* pwd = getpwuid(getuid());
        char* username = pwd->pw_name;
        char cwd[LENGTH];
        if (getcwd(cwd, LENGTH) == NULL) {
            perror("getcwd");
            exit(EXIT_FAILURE);
        }
        printf("\n");
        fflush(stdout);
        char* prompt = generate_prompt();

        char* input = readline(prompt);

        // 如果用户输入为空，则继续循环
        if (input == NULL || strcmp(input, "") == 0) {
            continue;
        }

        // 分解用户输入的命令和参数
        char* token;
        char* args[LENGTH];
        int argc = 0;
        token = strtok(input, " ");
        while (token != NULL && argc < LENGTH - 1) {
            args[argc++] = token;
            token = strtok(NULL, " ");
        }
        args[argc] = NULL;

        // 检查是否是退出命令
        if (strcmp(args[0], "exit") == 0) {
            free(input);
            break;
        }

        // 检查是否是改变目录命令
        if (strcmp(args[0], "cd") == 0) {
            if (argc == 1 || strcmp(args[1], "~") == 0) {
                chdir(getenv("HOME"));
            }
            else if (strcmp(args[1], "-") == 0) {
                chdir(last_dir);
            }
            else {
                chdir(args[1]);
            }
            strcpy(last_dir, cwd);
            free(input);
            continue;
        }
        my_execute(args, argc);
    }

    return 0;
}

// 执行命令
void my_execute(char** args, int argc)
{
    int background = 0;
    pid_t pid;
    pid = fork();
    if (pid == -1) {
        printf("无法创建子进程");
    }

    for (int i = 0; i < argc; i++) {
        if (strcmp(args[i], "&") == 0) {
            background = 1;
            args[i] = NULL;
            break;
        }
    }

    if (pid == 0) {
        do_pipe(0, argc, args);
    }
    else if (pid > 0) {
        if (background == 0) {
            waitpid(pid, NULL, 0);
        }
    }
}

// 处理重定向
void do_others(int left, int right, char** args)
{
    int fd;

    for (int i = left; i < right; i++) {
        if (args[i] == NULL) {
            continue;
        }

        if (strcmp(args[i], ">") == 0) { // args[i + 1]为重定向目标
            fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644); // 覆盖
            dup2(fd, STDOUT_FILENO);
            close(fd);
            args[i] = NULL;
            i++;
        }
        if (strcmp(args[i], ">>") == 0) { // args[i + 1]为重定向目标
            fd = open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0644); // 追加
            dup2(fd, STDOUT_FILENO);
            close(fd);
            args[i] = NULL;
            i++;
        }
        if (strcmp(args[i], "<") == 0) { // args[i + 1]为被重定向输入的目标
            fd = open(args[i + 1], O_RDONLY);
            dup2(
                fd,
                STDIN_FILENO); // 通过 dup2() 函数将文件描述符 fd 复制到标准输入 STDIN_FILENO 上
            close(fd);
            args[i] = NULL;
            i++;
        }
    }

    char* cmd[LENGTH];
    for (int i = left; i < right; i++) {
        cmd[i] = args[i];
    }
    cmd[right] = NULL;
    execvp(cmd[left], cmd + left);

    printf("无效命令\n");
    exit(EXIT_FAILURE);
}

// 处理管道
void do_pipe(int left, int right, char** args)
{
    int flag_pipe = -1;

    for (int i = left; i < right; i++) {
        if (args[i] == NULL) {
            continue;
        }
        if (strcmp(args[i], "|") == 0) {
            flag_pipe = i;
            break; // 当读取到管道标志符时，跳出循环处理管道
        }
    }

    if (flag_pipe == -1) { // 无管道时进入重定向处理函数
        do_others(left, right, args);
        exit(EXIT_SUCCESS);
    }

    if (flag_pipe + 1 == right) { // 管道在最后一个时的情况
        printf("|后面缺少参数\n");
        exit(EXIT_FAILURE);
    }

    int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    pid_t pid = fork();
    if (pid == -1) {
        printf("无法创建子进程");
    }

    if (pid == 0) {   // 子进程中
        close(fd[0]); // 关闭管道的读端
        dup2(
            fd[1],
            STDOUT_FILENO); // 通过 dup2() 函数将管道的写端 fd[1] 复制到标准输出 STDOUT_FILENO 上
        do_others(left, flag_pipe, args);
    }
    else if (pid > 0) { // 父进程中
        close(fd[1]);   // 关闭管道的写端
        dup2(
            fd[0],
            STDIN_FILENO); // 通过 dup2() 函数将管道的读端 fd[0] 复制到标准输入 STDIN_FILENO 上
        do_pipe(flag_pipe + 1, right, args);
    }
}
