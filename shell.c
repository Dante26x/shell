#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <pwd.h>
#include <assert.h>
#include <dirent.h>
#include "tokenizer.h"
#include <string.h>

//
/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_ls(struct tokens *tokens);
void my_ls(char *dirPath);
void my_cd(char *cmd);
void my_pwd();


#define MAXLINE 4096
#define MAXARG 10                       //限制参数表长度
#define MAX_DIR_DEPTH (128)             //限制pwd最大目录深度
#define STRUCT_PARSE_INFO
#define BACKGROUND 			1
#define IN_REDIRECT 		2
#define OUT_REDIRECT 		4
#define OUT_REDIRECT_APPEND	8
#define IS_PIPED 			16
#define MAXPIDTABLE 1024

char oldPath[128] = { 0 };              //用于记录上一次路径
char oldPwdPath[128] = { 0 };              //用于记录上一次路径
char *buffer;
pid_t BPTable[MAXPIDTABLE];

struct parse_info{                     //用于分析输入的指令
    int flag;
    char *in_file;
    char *out_file;
    char *command2;
    char **parameters2;
};

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
    cmd_fun_t *fun;
    char *cmd;
    char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
    {cmd_help, (char*)"?", (char*)"show this help menu"},
    {cmd_exit,(char*)"exit",(char*)"exit the command shell"},
    {cmd_cd,(char*)"cd",(char*)"change the directory"},
    {cmd_pwd,(char*)"pwd",(char*)"print work directory"},
    /*{cmd_ls,(char*)"ls",(char*)"list files"},*/
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens) {
    for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
        printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
    return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) {
    exit(0);
}

/*change the directory*/
int cmd_cd(unused struct tokens *tokens) {
    if (tokens_get_length(tokens) > 1)
        my_cd(strcat(strcat(tokens_get_token(tokens, 0), " "), tokens_get_token(tokens, 1)));
    else
        my_cd(tokens_get_token(tokens, 0));
    return 1;
}

/*print work directory*/
int cmd_pwd(unused struct tokens *tokens) {
    my_pwd();
    return 1;
}

/*list files*/
int cmd_ls(unused struct tokens *tokens) {
    my_ls((char*)".");
    return 1;
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
    for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
        if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
            return i;
    return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
    /* Our shell is connected to standard input. */
    shell_terminal = STDIN_FILENO;

    /* Check if we are running interactively */
    shell_is_interactive = isatty(shell_terminal);

    if (shell_is_interactive) {
        /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
         * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
         * foreground, we'll receive a SIGCONT. */
         /*while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
           kill(-shell_pgid, SIGTTIN);*/

         /* Saves the shell's process id */
        shell_pgid = getpid();

        /* Take control of the terminal */
        tcsetpgrp(shell_terminal, shell_pgid);

        /* Save the current termios to a variable, so it can be restored later. */
        tcgetattr(shell_terminal, &shell_tmodes);
    }
}



//printf_Flag
void printf_flag() {                          //打印提示信息  
    uid_t uid = getuid();
    char flag = '$';
    if (uid == 0)                         //如果是0，代表超级用户，用#标识符
    {
        flag = '#';
    }
    struct passwd *pw = getpwuid(uid);
    assert(pw != NULL);
    struct utsname hostname;
    uname(&hostname);
    char hname[128] = { 0 };
    strcpy(hname, hostname.nodename);
    int i = 0;
    while (hname[i] != '.')
        i++;
    hname[i] = '\0';
    //获得当前所在目录名
    char path[128] = { 0 };
    getcwd(path, 127);
    char *p = &path[strlen(path) - 1];    //将指针P指向path最后一个元素
    //解决在home目录下系统用~代替目录名的情况，pw_dir是passwd中提供的home目录信息
    if (strcmp(path, pw->pw_dir) == 0)
    {
        p = (char*)"~";
    }
    else if (strcmp(path, "/") == 0)     //解决在根目录下处理会忽视掉最右边的'/'的情况
    {
        p = path;
    }
    else {                              //普通情况处理
        while (*p != '/')
        {
            p--;
        }
        p++;
    }
    printf("myShell[%s@%s:/%s]%c ", pw->pw_name, hname, p, flag);
}


//my_cd
void my_cd(char *cmd) {
    // cd path
    // cd .. 上级目录
    // cd ~  home目录
    // cd - 上一次目录
    char nowPath[128] = { 0 };      //记录当前目录，用于上一次路径
    getcwd(nowPath, 127);
    char *p = cmd;
    while (*p != 0) {
        if (*p == ' ' && *(p + 1) != ' ') {
            p++;
            break;
        }
        p++;
    }
    if (strlen(p) == 0 || strcmp(p, "~") == 0) {   //cd后面没有参数或者跟着~，进入home目录
        struct passwd *pw = getpwuid(getuid());
        assert(pw != NULL);
        if (chdir(pw->pw_dir) == -1) {   //切换home目录失败退出，pw_dir进入home目录
            perror("cd ::");
            return;
        }
    }
    else if (strcmp(p, "-") == 0) {     //cd后面加-，进入上一次目录
        if (strlen(oldPath) == 0) {
            printf("oldPath not set\n");
            return;
        }
        else {
            if (chdir(oldPath) == -1) {   //切换失败退出
                perror("cd -::");
                return;
            }
        }
    }
    else {
        if (chdir(p) == -1) {
            perror("cd ::");
            return;
        }
    }
    strcpy(oldPath, nowPath);
}

//my_pwd
ino_t get_ino_byname(char *filename) {  //根据文件名获取文件inode-number
    struct stat file_stat;
    if (stat(filename, &file_stat) != 0) {
        perror("stat");
        return -1;
    }
    return file_stat.st_ino;
}

char* find_name_byino(ino_t ino) {    //根据inode-number，在当前目录中查找对应的文件名
    DIR* dp = NULL;
    struct dirent* dptr = NULL;
    char* filename = NULL;
    if ((dp = opendir(".")) == NULL) {
        fprintf(stderr, "Can not open Current Directory\n");
        return NULL;
    }
    else {
        while ((dptr = readdir(dp)) != NULL) {
            if (dptr->d_ino == ino) {
                filename = strdup(dptr->d_name);
                break;
            }
        }
        closedir(dp);
    }
    return filename;
}

void my_pwd() {
    char *dir_stack[MAX_DIR_DEPTH];             //记录目录名的栈
    unsigned current_depth = 0;
    getcwd(oldPwdPath, 127);                       //记录当前目录
    for (;;) {
        ino_t current_ino = get_ino_byname((char*)".");     //1.先通过特殊的文件名获取当前目录的inode-number
        ino_t parent_ino = get_ino_byname((char*)"..");     //2.再通过特殊文件名获取当前目录的上一层目录
        if (current_ino == parent_ino)               //3.当当前目录与上一级目录相同时说明到达根目录
            break;
        chdir("..");                                 //4.不同时则切换到上一级目录
        dir_stack[current_depth++] = find_name_byino(current_ino);
        if (current_depth >= MAX_DIR_DEPTH) {        //路径太深
            fprintf(stderr, "Directory tree is too deep.\n");
            return;
        }
    }
    int i = current_depth - 1;                 //输出完整路径
    for (; i >= 0; i--)
    {
        fprintf(stdout, "/%s", dir_stack[i]);
    }
    fprintf(stdout, "%s\n", current_depth == 0 ? "/" : "");
    chdir(oldPwdPath);                            //从根目录回到当前目录
    return;
}

//my_ls
void my_ls(char *dirpath) {
    DIR *pDir;
    struct dirent *pDirent;
    pDir = opendir(dirpath);
    //printf("%s\n", dirpath);
    if ( pDir == NULL) 
    {
        fprintf(stderr, "ls:can not open %s\n", dirpath);
    }
    else {
        while ((pDirent = readdir(pDir)) != NULL) {
            if (strcmp(pDirent->d_name, ".") == 0 || strcmp(pDirent->d_name, "..") == 0) 
            {
                continue;
            }
            printf("%s\n", pDirent->d_name);
        }
        closedir(pDir);
    }
}

//程序执行
int read_command(char **command, char **parameters, char *prompt){
    strcpy(buffer, prompt);
    if (buffer[0] == '\0')
        return -1;
    char *pStart, *pEnd;
    int count = 0;
    int isFinished = 0;
    pStart = pEnd = buffer;
    while (isFinished == 0){
        while ((*pEnd == ' ' && *pStart == ' ') || (*pEnd == '\t' && *pStart == '\t')){
            pStart++;
            pEnd++;
        }
        if (*pEnd == '\0' || *pEnd == '\n'){
            if (count == 0)
                return -1;
            break;
        }
        while (*pEnd != ' ' && *pEnd != '\0' && *pEnd != '\n')
            pEnd++;
        if (count == 0){
            char *p = pEnd;
            *command = pStart;
            while (p != pStart && *p != '/')
                p--;
            if (*p == '/')
                p++;
            //else //p==pStart
            parameters[0] = p;
            count += 2;
#ifdef DEBUG
            printf("\ncommand:%s\n", *command);
#endif
        }
        else if (count <= MAXARG) {
            parameters[count - 1] = pStart;
            count++;
        }
        else
        {
            break;
        }

        if (*pEnd == '\0' || *pEnd == '\n'){
            *pEnd = '\0';
            isFinished = 1;
        }
        else{
            *pEnd = '\0';
            pEnd++;
            pStart = pEnd;
        }
    }
    parameters[count - 1] = NULL;
#ifdef DEBUG
    /*input analysis*/
    printf("input analysis:\n");
    printf("pathname:[%s]\ncommand:[%s]\nparameters:\n", *command, parameters[0]);
    int i;
    for (i = 0; i < count - 1; i++)
        printf("[%s]\n", parameters[i]);
#endif
    return count;
}

//指令分析
int parse_info_init(struct parse_info *info){   //指令分析初始化
    info->flag = 0;
    info->in_file = NULL;
    info->out_file = NULL;
    info->command2 = NULL;
    info->parameters2 = NULL;
    return 0;
}

int parsing(char **parameters, int ParaNum, struct parse_info *info){
    int i;
    parse_info_init(info);
    if (strcmp(parameters[ParaNum - 1], "&") == 0) {
        info->flag |= BACKGROUND;
        parameters[ParaNum - 1] = NULL;
        ParaNum--;
    }
    for (i = 0; i < ParaNum;) {
        if (strcmp(parameters[i], "<<") == 0 || strcmp(parameters[i], "<") == 0) {
            info->flag |= IN_REDIRECT;
            info->in_file = parameters[i + 1];
            parameters[i] = NULL;
            i += 2;
        }
        else if (strcmp(parameters[i], ">") == 0) {
            info->flag |= OUT_REDIRECT;
            info->out_file = parameters[i + 1];
            parameters[i] = NULL;
            i += 2;
        }
        else if (strcmp(parameters[i], ">>") == 0) {
            info->flag |= OUT_REDIRECT_APPEND;
            info->out_file = parameters[i + 1];
            parameters[i] = NULL;
            i += 2;
        }
        else if (strcmp(parameters[i], "|") == 0) {
            char* pCh;
            info->flag |= IS_PIPED;
            parameters[i] = NULL;
            info->command2 = parameters[i + 1];
            info->parameters2 = &parameters[i + 1];
            for (pCh = info->parameters2[0] + strlen(info->parameters2[0]);
                pCh != &(info->parameters2[0][0]) && *pCh != '/'; pCh--)
                ;
            if (*pCh == '/')
                pCh++;
            info->parameters2[0] = pCh;
            break;
        }
        else
            i++;
    }
    #ifdef DEBUG
        printf("\nbackground:%s\n", info->flag&BACKGROUND ? "yes" : "no");
        printf("in redirect:");
        info->flag&IN_REDIRECT ? printf("yes,file:%s\n", info->in_file) : printf("no\n");
        printf("out redirect:");
        info->flag&OUT_REDIRECT ? printf("yes,file:%s\n", info->out_file) : printf("no\n");
        printf("pipe:");
        info->flag&IS_PIPED ? printf("yes,command:%s %s %s\n", info->command2, info->parameters2[0], info->parameters2[1]) : printf("no\n");
    #endif
    return 1;
}

//处理后台程序
void sig_handler(int sig){
    pid_t pid;
    int i;
    for (i = 0; i < MAXPIDTABLE; i++)
        if (BPTable[i] != 0) {//only handler the background processes
            pid = waitpid(BPTable[i], NULL, WNOHANG);
            if (pid > 0) {
                printf("process %d exited.\n", pid);
                BPTable[i] = 0; //clear
            }
            else if (pid < 0){
                if (errno != ECHILD)
                    perror("waitpid error");
            }
            //else:do nothing.
            //Not background processses has their waitpid() in wshell.
        }
    return;
}

//处理路径
char *ls(char *path, char *name,char *tpath)
{
    char apath[1000];
    //char bpath[1000];
    DIR *dir;           //千万不能设为全局变量
    char newpath[1000];
    struct dirent *ptr;
    if (path == NULL)
        return NULL;
    dir = opendir(path);
    if (dir != NULL){
        while ((ptr = readdir(dir)) != NULL){
            if (strcmp(ptr->d_name, ".") != 0 && strcmp(ptr->d_name, "..") != 0 && strcmp(ptr->d_name, name) == 0){  //筛选出同名文件
                strcpy(apath, path);
                if (apath[strlen(apath) - 1] == '/')
                    strcat(apath, ptr->d_name);
                else
                    sprintf(apath, "%s/%s", apath, ptr->d_name);
                //printf("%s\n", apath);
                strcpy(tpath, apath);
                break;
                //++count;
            }

        }
        closedir(dir);
    }
}

char *get_path(char *file) {
    getcwd(oldPwdPath, 127);
    char *path;
    path = (char*)malloc(sizeof(char*) * (MAXARG + 2));
    char *pathvar;
    char *ptr;
    pathvar = getenv("PATH");
    char *next_token = NULL;
    ptr = strtok(pathvar, ":");
    if (NULL != ls(ptr, file, path))
        printf("%s\n", ls(ptr, file, path));
        //return ls(ptr, file);
    //chdir(pathvar);
    while (ptr != NULL) {
        ptr = strtok(NULL, ":");
        if (NULL != ls(ptr, file, path))
            //return ls(ptr, file);
            printf("%s\n", ls(ptr, file, path));
    }
    return path;
}

int main(unused int argc, unused char *argv[]) {
    init_shell();
    int j;
    //init the BPTable
    for (j = 0; j < MAXPIDTABLE; j++)
        BPTable[j] = 0;
    int flag = 0;
    static char line[4096];
    int line_num = -1;
    int status;
    char *command;
    char **parameters;
    struct parse_info info;
    pid_t ChdPid, ChdPid2;
    int ParaNum;
    parameters = (char**)malloc(sizeof(char*) * (MAXARG + 2));
    buffer = malloc(sizeof(char) * MAXLINE);
    /* Please only print shell prompts when standard input is not a tty */
    if (signal(SIGCHLD, sig_handler) == SIG_ERR)
        perror("signal() error");
    while (true) {
        flag = 1;
        if (shell_is_interactive) {
            fprintf(stdout, "%d: ", ++line_num);
            printf_flag();
        }
        fgets(line, 4096, stdin);
        int pipe_fd[2], in_fd, out_fd;
        /* Split our line into words. */
        struct tokens *tokens = tokenize(line);
        ParaNum = read_command(&command, parameters, line);
        /* Find which built-in function to run. */
        int fundex = lookup(tokens_get_token(tokens, 0));
        if (-1 == ParaNum) {
            printf("%d", ParaNum);
            continue;
        } 
        ParaNum--;
        parsing(parameters, ParaNum, &info);
        if (fundex >= 0) {
            //printf("A\n");
            cmd_table[fundex].fun(tokens);
            continue;
        }
        //printf("%s\n",command);
        if (info.flag & IS_PIPED) {//command2 is not null
            if (pipe(pipe_fd) < 0) {
                printf("Wshell error:pipe failed.\n");
                exit(0);
            }
        }
        if(command[0]=='.')
            *command++;
        //printf("%s\n",command);
        if ((ChdPid = fork()) != 0) { //wshell 
            if (info.flag & IS_PIPED) {
                if ((ChdPid2 = fork()) == 0) {//command2 
                    close(pipe_fd[1]);
                    close(fileno(stdin));
                    dup2(pipe_fd[0], fileno(stdin));
                    close(pipe_fd[0]);
                    if(info.command2[0]=='/')
                        execv(info.command2, info.parameters2);
                    else
                        execv(get_path(info.command2), info.parameters2);
                }
                else {
                    close(pipe_fd[0]);
                    close(pipe_fd[1]);
                    waitpid(ChdPid2, &status, 0); //wait command2
                }
            }
            if (info.flag & BACKGROUND)   {
                printf("Child pid:%u\n", ChdPid);
                int i;
                for (i = 0; i < MAXPIDTABLE; i++)
                    if (BPTable[i] == 0)
                        BPTable[i] = ChdPid; //register a background process
                if (i == MAXPIDTABLE)
                    perror("Too much background processes\nThere will be zombine process");
            }
            else
                waitpid(ChdPid, &status, 0);//wait command1
        }
        else { //command1
            if (info.flag & IS_PIPED) { //command2 is not null
                if (!(info.flag & OUT_REDIRECT) && !(info.flag & OUT_REDIRECT_APPEND)) {// ONLY PIPED
                    close(pipe_fd[0]);
                    close(fileno(stdout));
                    dup2(pipe_fd[1], fileno(stdout));
                    close(pipe_fd[1]);
                }
                else {//OUT_REDIRECT and PIPED
                    close(pipe_fd[0]);
                    close(pipe_fd[1]);//send a EOF to command2
                    if (info.flag & OUT_REDIRECT)
                        out_fd = open(info.out_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    else
                        out_fd = open(info.out_file, O_WRONLY | O_APPEND | O_TRUNC, 0666);
                    close(fileno(stdout));
                    dup2(out_fd, fileno(stdout));
                    close(out_fd);
                }
            }
            else {
                if (info.flag & OUT_REDIRECT) {// OUT_REDIRECT WITHOUT PIPE
                    out_fd = open(info.out_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    close(fileno(stdout));
                    dup2(out_fd, fileno(stdout));
                    close(out_fd);
                }
                if (info.flag & OUT_REDIRECT_APPEND) {// OUT_REDIRECT_APPEND WITHOUT PIPE 
                    out_fd = open(info.out_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
                    close(fileno(stdout));
                    dup2(out_fd, fileno(stdout));
                    close(out_fd);
                }
            }
            if (info.flag & IN_REDIRECT) {
                in_fd = open(info.in_file, O_CREAT | O_RDONLY, 0666);
                close(fileno(stdin));
                dup2(in_fd, fileno(stdin));
                close(in_fd);
            }
            if(command[0]=='/')
                execv(command, parameters);
            else
                if(execv(get_path(command), parameters)==-1){
                    printf("unknown command\n");
                    exit(-1);
                }
        }
        tokens_destroy(tokens);    
    }
    free(parameters);
    free(buffer);
    return 0;
}
