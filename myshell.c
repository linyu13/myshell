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

char last_dir[LENGTH] = "";
char* generate_prompt();

void signal_handler(int signum)
{
    printf("\n");
    fflush(stdout);
}

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
    // signal(SIGTSTP, signal_handler);
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

        // 将用户输入添加到历史记录
        // add_history(input);

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

        // 检查重定向符号并进行重定向
        int redirect_out = 0; // 标记是否有输出重定向
        int redirect_out_app = 0;
        int redirect_out_over = 0;
        int redirect_in = 0; // 标记是否有输入重定向
        char* output_file = NULL;
        char* input_file = NULL;

        for (int i = 0; i < argc; i++) {
            if (strcmp(args[i], ">") == 0) {
                if (i + 1 < argc) {
                    redirect_out = 1;
                    redirect_out_over = 1;
                    output_file = args[i + 1];
                    args[i] = NULL;
                    args[i + 1] = NULL;
                    break;
                }
            }
            else if (strcmp(args[i], ">>") == 0) {
                if (i + 1 < argc) {
                    redirect_out = 1; // 追加重定向
                    redirect_out_app = 1;
                    output_file = args[i + 1];
                    args[i] = NULL;
                    args[i + 1] = NULL;
                    break;
                }
            }
            else if (strcmp(args[i], "<") == 0) {
                if (i + 1 < argc) {
                    redirect_in = 1;
                    input_file = args[i + 1];
                    args[i] = NULL;
                    args[i + 1] = NULL;
                    break;
                }
            }
        }

        // 创建子进程执行命令
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            free(input);
            exit(EXIT_FAILURE);
        }
        else if (pid == 0) {
            // 子进程执行命令
            if (redirect_out) {
                int fd;
                if (redirect_out_over) { // 覆盖重定向
                    fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                }
                else if (redirect_out_app) { // 追加重定向
                    fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
                }
                if (fd < 0) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            if (redirect_in) {
                int fd = open(input_file, O_RDONLY);
                if (fd < 0) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            execvp(args[0], args);
            perror("execvp");
            free(input);
            exit(EXIT_FAILURE);
        }
        else {
            // 等待子进程执行完成
            wait(NULL);
        }

        free(input);
    }

    return 0;
}