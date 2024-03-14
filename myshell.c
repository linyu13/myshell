#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
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

char last_dir[1000] = "";
int flag_linyu = 0;
void signal_handler(int signum)
{
    printf("\n");
    fflush(stdout);
    // 获取当前用户名
    flag_linyu = 1;
    struct passwd* pwd = getpwuid(getuid());
    char* buf = (char*)calloc(LENGTH, sizeof(char)); // 分配内存并清零
    if (buf == NULL) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }
    if (getcwd(buf, LENGTH) == NULL) {
        perror("getcwd");
        free(buf);
        exit(EXIT_FAILURE);
    }
    printf(
        "\n" BLUE BOLD "#\033[0m " BLUE "%s\033[0m " WHITE "@\033[0m " GREEN
        "linyu\033[0m in " YELLOW "%s\033[0m\n" RED BOLD "$ \033[0m",
        pwd->pw_name,
        buf);

    fflush(stdout); // 打印提示符并刷新输出缓冲区
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
        // 获取当前用户名
        struct passwd* pwd = getpwuid(getuid());
        char* buf = (char*)calloc(LENGTH, sizeof(char)); // 分配内存并清零
        if (!flag_linyu) {
            if (buf == NULL) {
                perror("calloc");
                exit(EXIT_FAILURE);
            }
            if (getcwd(buf, LENGTH) == NULL) {
                perror("getcwd");
                free(buf);
                exit(EXIT_FAILURE);
            }
            printf(
                BLUE BOLD "#\033[0m " BLUE "%s\033[0m " WHITE "@\033[0m " GREEN
                          "linyu\033[0m in " YELLOW "%s\033[0m\n" RED BOLD "$ \033[0m",
                pwd->pw_name,
                buf);

            fflush(stdout); // 打印提示符并刷新输出缓冲区
        }
        // 定义字符串存储输入的信息
        char arr[100] = { 0 };
        // 将信息输入到arr中
        fgets(arr, 99, stdin);
        // 将最后一个字符改为\0（将回车改为\0表示字符串结束）
        arr[strlen(arr) - 1] = '\0';

        // 处理字符串//
        int i = 0;
        char* argv = arr;
        char* str[100] = { NULL };
        if (strlen(arr) == 0 || 0 == strcmp("\n", arr)) {
            continue;
        }

        while (*argv != '\0')  // 字符串没有结束之前一直循环
        {
            if (*argv != ' ')  // 字符不是空格时
            {
                str[i] = argv; // 将字符写入str中
                i++;
                // 当前字符不是空格并且字符串未结束遍历下一字符
                // 当字符为空格时，将该字符转换为结束符'\0'（字符命令结束）
                // 开始下一个字符
                while (*argv != '\0' && *argv != ' ') {
                    argv++;
                }
                *argv = '\0';
            }
            argv++;
        }
        // 将下一指针设置为空
        str[i] = NULL;
        if (strcmp("exit", str[0]) == 0) {
            break;
        }
        // 当出现cd的时候,这里是字符串的比较,strcmp函数,相同则为0
        if (strcmp("cd", str[0]) == 0) {
            if (str[1] == NULL || str[1] == 0) {
                chdir(getenv("HOME"));
                strcpy(last_dir, buf);
            }
            else if (strcmp(str[1], "-") == 0) {
                if (strlen(last_dir) == 0) {
                    printf("%s\n", buf);
                }
                else {
                    if (chdir(last_dir) == -1) {
                        perror("chdir");
                    }
                    else {
                        printf("%s\n", last_dir);
                    }
                    strcpy(last_dir, buf);
                }
            }
            else {
                // char cwd[1024];
                // if (getcwd(cwd, sizeof(cwd)) != NULL) {
                //     printf("当前工作目录：%s\n", cwd);
                // }
                if (chdir(str[1]) == -1) {
                    perror("chdir");
                }
                strcpy(last_dir, buf);
            }
            continue;
        }
        // 创建子程序//
        pid_t pid = fork();
        // 创建失败
        if (pid < 0) {
            perror("error");
            return -1;
        }
        else if (0 == pid) {
            // 使用cxecvp进行替换，转到需要执行的程序中
            execvp(str[0], str);
            perror("error");
            exit(-1);
        }
        else if (strcmp(str[i - 1], "&") != 0) {
            wait(NULL);
        }
    }
    return 0;
}
