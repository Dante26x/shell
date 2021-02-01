[toc]

# shell

## 1.	Data structures and functions

### 1.指令信息

struct parse_info{                 //用于分析输入的指令

​    int flag;

​    char *in_file;

​    char *out_file;

​    char *command2;

​    char **parameters2;

};

## 2.Algorithms

### 1.打印终端提示符

//printf_Flag

void printf_flag() {                          //打印提示信息  

​    uid_t uid = getuid();

​    char flag = '$';

​    if (uid == 0)                         //如果是0，代表超级用户，用#标识符

​    {

​        flag = '#';

​    }

​    struct passwd *pw = getpwuid(uid);

​    assert(pw != NULL);

​    struct utsname hostname;

​    uname(&hostname);

​    char hname[128] = { 0 };

​    strcpy(hname, hostname.nodename);

​    int i = 0;

​    while (hname[i] != '.')

​        i++;

​    hname[i] = '\0';

​    //获得当前所在目录名

​    char path[128] = { 0 };

​    getcwd(path, 127);

​    char *p = &path[strlen(path) - 1];    //将指针P指向path最后一个元素

​    //解决在home目录下系统用~代替目录名的情况，pw_dir是passwd中提供的home目录信息

​    if (strcmp(path, pw->pw_dir) == 0)

​    {

​        p = (char*)"~";
​    }*


​    else if (strcmp(path, "/") == 0)     //解决在根目录下处理会忽视掉最右边的'/'的情况

​    {

​        p = path;

​    }
​    else {                              //普通情况处理

​        while (*p != '/')

​        {

​            p--;

​        }

​        p++;

​    }

​    printf("myShell[%s@%s:/%s]%c ", pw->pw_name, hname, p, flag);

}

### 2.exit指令

int cmd_exit(unused struct tokens *tokens) {

​    exit(0);

}

### 3.cd指令

#### 1.指令调用

/*change the directory*/

int cmd_cd(unused struct tokens *tokens) {

​    if (tokens_get_length(tokens) > 1)  //判断cd指令后有没有参数

​        my_cd(strcat(strcat(tokens_get_token(tokens, 0), " "), tokens_get_token(tokens, 1))); //拼接指令

​    else

​        my_cd(tokens_get_token(tokens, 0));

​    return 1;

}

#### 2.指令实现

//my_cd

void my_cd(char *cmd) {

​    // cd path

​    // cd .. 上级目录

​    // cd ~  home目录

​    // cd - 上一次目录

​    char nowPath[128] = { 0 };      //记录当前目录，用于上一次路径

​    getcwd(nowPath, 127);

​    char *p = cmd;*

​    while (*p != 0) {*

​        if (*p == ' ' && *(p + 1) != ' ') {

​            p++;

​            break;

​        }

​        p++;

​    }

​    if (strlen(p) == 0 || strcmp(p, "~") == 0) {   //cd后面没有参数或者跟着~，进入home目录

​        struct passwd *pw = getpwuid(getuid());

​        assert(pw != NULL);

​        if (chdir(pw->pw_dir) == -1) {   //切换home目录失败退出，pw_dir进入home目录

​            perror("cd ::");

​            return;

​        }

​    }

​    else if (strcmp(p, "-") == 0) {     //cd后面加-，进入上一次目录

​        if (strlen(oldPath) == 0) {

​            printf("oldPath not set\n");

​            return;

​        }

​        else {

​            if (chdir(oldPath) == -1) {   //切换失败退出

​                perror("cd -::");

​                return;

​            }

​        }

​    }

​    else {

​        if (chdir(p) == -1) {

​            perror("cd ::");

​            return;

​        }

​    }

​    strcpy(oldPath, nowPath);

}

### 4.pwd指令

#### 1.指令调用

int cmd_pwd(unused struct tokens *tokens) {

​    my_pwd();

​    return 1;

}

#### 2.指令实现



char* find_name_byino(ino_t ino) {    //根据inode-number，在当前目录中查找对应的文件名

​    DIR* dp = NULL;

​    struct dirent* dptr = NULL;

​    char* filename = NULL;

​    if ((dp = opendir(".")) == NULL) {

​        fprintf(stderr, "Can not open Current Directory\n");

​        return NULL;

​    }

​    else {

​        while ((dptr = readdir(dp)) != NULL) {

​            if (dptr->d_ino == ino) {

​                filename = strdup(dptr->d_name);

​                break;

​            }

​        }

​        closedir(dp);

​    }

​    return filename;

}

#### 3.get_ino_byname函数实现

ino_t get_ino_byname(char *filename) {  //根据文件名获取文件inode-number

​    struct stat file_stat;

​    if (stat(filename, &file_stat) != 0) {

​        perror("stat");

​        return -1;

​    }

​    return file_stat.st_ino;

}

#### 4.find_name_byino函数实现

char* find_name_byino(ino_t ino) {    //根据inode-number，在当前目录中查找对应的文件名

​    DIR* dp = NULL;

​    struct dirent* dptr = NULL;

​    char* filename = NULL;

​    if ((dp = opendir(".")) == NULL) {

​        fprintf(stderr, "Can not open Current Directory\n");

​        return NULL;

​    }

​    else {

​        while ((dptr = readdir(dp)) != NULL) {

​            if (dptr->d_ino == ino) {

​                filename = strdup(dptr->d_name);

​                break;

​            }

​        }

​        closedir(dp);

​    }

​    return filename;

}

### 5.分析读取命令与参数数量

int read_command(char **command, char **parameters, char *prompt){

​    strcpy(buffer, prompt);

​    if (buffer[0] == '\0')

​        return -1;

​    char *pStart, *pEnd;*

​    int count = 0;

​    int isFinished = 0;

​    pStart = pEnd = buffer;

​    while (isFinished == 0){

​        while ((*pEnd == ' ' && *pStart == ' ') || (*pEnd == '\t' && *pStart == '\t')){*

​            pStart++;

​            pEnd++;

​        }

​        if (*pEnd == '\0' || *pEnd == '\n'){*

​            if (count == 0)

​                return -1;

​            break;

​        }

​        while (*pEnd != ' ' && *pEnd != '\0' && *pEnd != '\n')

​            pEnd++;

​        if (count == 0){

​            char *p = pEnd;

​            *command = pStart;

​            while (p != pStart && *p != '/')*

​                p--;

​            if (*p == '/')

​                p++;

​            //else //p==pStart

​            parameters[0] = p;

​            count += 2;

#ifdef DEBUG

​            printf("\ncommand:%s\n", *command);

#endif

​        }

​        else if (count <= MAXARG) {

​            parameters[count - 1] = pStart;

​            count++;

​        }

​        else

​        {

​            break;

​        }



​    if (*pEnd == '\0' || *pEnd == '\n'){

​        *pEnd = '\0';

​        isFinished = 1;

​    }

​    else

​    {

​        *pEnd = '\0';

​        pEnd++;

​        pStart = pEnd;

​    }

}

parameters[count - 1] = NULL;



#ifdef DEBUG

​    /*input analysis*/

​    printf("input analysis:\n");

​    printf("pathname:[%s]\ncommand:[%s]\nparameters:\n", *command, parameters[0]);

​    int i;

​    for (i = 0; i < count - 1; i++)

​        printf("[%s]\n", parameters[i]);

#endif

​    return count;

}

### 6.指令分析

#### 1.指令分析初始化



int parse_info_init(struct parse_info *info){ 

​    info->flag = 0;

​    info->in_file = NULL;

​    info->out_file = NULL;

​    info->command2 = NULL;

​    info->parameters2 = NULL;

​    return 0;

}

#### 2.指令分析

int parsing(char **parameters, int ParaNum, struct parse_info *info){*

​    int i;

​    parse_info_init(info);

  ***分析指令末尾是否有&，判断是否后台运行***



​    if (strcmp(parameters[ParaNum - 1], "&") == 0) {  

​        info->flag |= BACKGROUND;

​        parameters[ParaNum - 1] = NULL;

​        ParaNum--;

​    }

​    for (i = 0; i < ParaNum;) {

***判断是否重定向***



​        if (strcmp(parameters[i], "<<") == 0 || strcmp(parameters[i], "<") == 0) {

​            info->flag |= IN_REDIRECT;

​            info->in_file = parameters[i + 1];

​            parameters[i] = NULL;

​            i += 2;

​        }

​        else if (strcmp(parameters[i], ">") == 0) {

​            info->flag |= OUT_REDIRECT;

​            info->out_file = parameters[i + 1];

​            parameters[i] = NULL;

​            i += 2;

​        }

​        else if (strcmp(parameters[i], ">>") == 0) {

​            info->flag |= OUT_REDIRECT_APPEND;

​            info->out_file = parameters[i + 1];

​            parameters[i] = NULL;

​            i += 2;

​        }

***判断是否使用管道***



​        else if (strcmp(parameters[i], "|") == 0) {

​            char* pCh;

​            info->flag |= IS_PIPED;

​            parameters[i] = NULL;

​            info->command2 = parameters[i + 1];

​            info->parameters2 = &parameters[i + 1];

​            for (pCh = info->parameters2[0] + strlen(info->parameters2[0]);

​                pCh != &(info->parameters2[0][0]) && *pCh != '/'; pCh--)*

​            if (*pCh == '/')

​                pCh++;

​            info->parameters2[0] = pCh;

​            break;

​        }

​        else

​            i++;

​    }

​    #ifdef DEBUG

​        printf("\nbackground:%s\n", info->flag&BACKGROUND ? "yes" : "no");

​        printf("in redirect:");

​        info->flag&IN_REDIRECT ? printf("yes,file:%s\n", info->in_file) : printf("no\n");

​        printf("out redirect:");

​        info->flag&OUT_REDIRECT ? printf("yes,file:%s\n", info->out_file) : printf("no\n");

​        printf("pipe:");

​        info->flag&IS_PIPED ? printf("yes,command:%s %s %s\n", info->command2, info->parameters2[0], info->parameters2[1]) : printf("no\n");

​    #endif

​    return 1;

}

### 7.处理后台程序



void sig_handler(int sig){

​    pid_t pid;

​    int i;

​    for (i = 0; i < MAXPIDTABLE; i++)

​        if (BPTable[i] != 0) {//**only handler the background processes**

​            pid = waitpid(BPTable[i], NULL, WNOHANG);

​            if (pid > 0) {

​                printf("process %d exited.\n", pid);

​                BPTable[i] = 0; //clear

​            }

​            else if (pid < 0){

​                if (errno != ECHILD)

​                    perror("waitpid error");

​            }

​            //else:do nothing.

​            //Not background processses has their waitpid() in wshell.

​        }

​    return;

}

### 8.路径解析

#### 1.指令实现

char *get_path(char *file) {

​    getcwd(oldPwdPath, 127);

​    char *path;

​    path = (char*)malloc(sizeof(char*) * (MAXARG + 2));

​    char *pathvar;    char *ptr;

***获得系统环境变量***

​    pathvar = getenv("PATH");

​    char *next_token = NULL;

***分解环境变量并查找指令***

​    ptr = strtok(pathvar, ":");

​    if (NULL != ls(ptr, file, path))

​        printf("%s\n", ls(ptr, file, path));

​        //return ls(ptr, file);

​    //chdir(pathvar);

​    while (ptr != NULL) {

​        ptr = strtok(NULL, ":");

​        if (NULL != ls(ptr, file, path))

​            //return ls(ptr, file);

​            printf("%s\n", ls(ptr, file, path));

​    }

​    return path;

}

#### 2.ls函数实现

char *ls(char *path, char *name,char *tpath)

{

​    char apath[1000];

​    DIR *dir;           //千万不能设为全局变量

​    char newpath[1000];

​    struct dirent *ptr;

​    if (path == NULL)

​        return NULL;

​    dir = opendir(path);

​    if (dir != NULL){

​        while ((ptr = readdir(dir)) != NULL){

***筛选出同名文件***

if (strcmp(ptr->d_name, ".") != 0 && strcmp(ptr->d_name, "..") != 0 && strcmp(ptr->d_name, name) == 0){  

​                strcpy(apath, path);

​                if (apath[strlen(apath) - 1] == '/')

​                    strcat(apath, ptr->d_name);

​                else

​                    sprintf(apath, "%s/%s", apath, ptr->d_name);

​                //printf("%s\n", apath);  //打印绝对地址

​                strcpy(tpath, apath);

​                break;

​                //++count;

​            }



​    }

​    closedir(dir);

}

}

### 9.main函数

`int main(unused int argc, unused char *argv[]) {`

 `init_shell();`

 `int j;`
    ***后台初始化***

``for (j = 0; j < MAXPIDTABLE; j++)`
   `BPTable[j] = 0;`
    `int flag = 0;`
    `static char line[4096];`
    `int line_num = -1;`
    `int status;`
    `char *command;
  char **parameters;
  struct parse_info info;
  pid_t ChdPid, ChdPid2;
  int ParaNum;
  parameters = (char**)malloc(sizeof(char*) * (MAXARG + 2));`
    `buffer = malloc(sizeof(char) * MAXLINE);`
 `/* Please only print shell prompts when standard input is not a tty */
    if (signal(SIGCHLD, sig_handler) == SIG_ERR)
        perror("signal() error");
    while (true) {
        flag = 1;`

​	`***打印终端提示符***`

`if (shell_is_interactive) {
            fprintf(stdout, "%d: ", ++line_num);
            printf_flag();
        }
        fgets(line, 4096, stdin);
        int pipe_fd[2], in_fd, out_fd;
        /* Split our line into words. */`
        `struct tokens *tokens = tokenize(line);
        ParaNum = read_command(&command, parameters, line);
        /* Find which built-in function to run. */`
        `int fundex = lookup(tokens_get_token(tokens, 0));`
        `if (-1 == ParaNum) {`
            `printf("%d", ParaNum);`
            `continue;`
        `}` 
        `ParaNum--;`
        `parsing(parameters, ParaNum, &info);`

 `***执行内建指令***`

​        `if (fundex >= 0) {`
`​            printf("A\n");`
`​            cmd_table[fundex].fun(tokens);`
`​            continue;`
`​        }`

`***执行command2***`

`***同时判断是否重定向和管道***`

​        `if (info.flag & IS_PIPED) {//command2 is not null`
`​            if (pipe(pipe_fd) < 0) {`
`​                printf("Wshell error:pipe failed.\n");`
`​                exit(0);`
`​            }`
`​        }`
`​        if ((ChdPid = fork()) != 0) { //wshell` 
`​            if (info.flag & IS_PIPED) {`
`​                if ((ChdPid2 = fork()) == 0) {//command2` 
`​                    close(pipe_fd[1]);`
`​                    close(fileno(stdin));`
`​                    dup2(pipe_fd[0], fileno(stdin));`
`​                    close(pipe_fd[0]);`
`​                    execv(get_path(info.command2), info.parameters2);`
`​                }`
`​                else {`
`​                    close(pipe_fd[0]);`
`​                    close(pipe_fd[1]);`
`​                    waitpid(ChdPid2, &status, 0); //wait command2`
`​                }`
`​            }`
`​            if (info.flag & BACKGROUND)   {`
`​                printf("Child pid:%u\n", ChdPid);`
`​                int i;`
`​                for (i = 0; i < MAXPIDTABLE; i++)`
`​                    if (BPTable[i] == 0)`
`​                        BPTable[i] = ChdPid; //register a background process`
`​                if (i == MAXPIDTABLE)`
`​                    perror("Too much background processes\nThere will be zombine process");`
`​            }`
`​            else`
`​                waitpid(ChdPid, &status, 0);//wait command1`
`​        }`
`​        else { //command1`

`***执行command1***`

​            `if (info.flag & IS_PIPED) { //command2 is not null`
`​                if (!(info.flag & OUT_REDIRECT) && !(info.flag & OUT_REDIRECT_APPEND)) {// ONLY PIPED`
`​                    close(pipe_fd[0]);`
`​                    close(fileno(stdout));`
`​                    dup2(pipe_fd[1], fileno(stdout));`
`​                    close(pipe_fd[1]);`
`​                }`
`​                else {//OUT_REDIRECT and PIPED`
`​                    close(pipe_fd[0]);`
`​                    close(pipe_fd[1]);//send a EOF to command2`
`​                    if (info.flag & OUT_REDIRECT)`
`​                        out_fd = open(info.out_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);`
`​                    else`
`​                        out_fd = open(info.out_file, O_WRONLY | O_APPEND | O_TRUNC, 0666);`
`​                    close(fileno(stdout));`
`​                    dup2(out_fd, fileno(stdout));`
`​                    close(out_fd);`
`​                }`
`​            }`
`​            else {`
`​                if (info.flag & OUT_REDIRECT) {// OUT_REDIRECT WITHOUT PIPE`
`​                    out_fd = open(info.out_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);`
`​                    close(fileno(stdout));`
`​                    dup2(out_fd, fileno(stdout));`
`​                    close(out_fd);`
`​                }`
`​                if (info.flag & OUT_REDIRECT_APPEND) {// OUT_REDIRECT_APPEND WITHOUT PIPE` 
`​                    out_fd = open(info.out_file, O_WRONLY | O_CREAT | O_APPEND, 0666);`
`​                    close(fileno(stdout));`
`​                    dup2(out_fd, fileno(stdout));`
`​                    close(out_fd);`
`​                }`
`​            }`
`​            if (info.flag & IN_REDIRECT) {`
`​                in_fd = open(info.in_file, O_CREAT | O_RDONLY, 0666);`
`​                close(fileno(stdin));`
`​                dup2(in_fd, fileno(stdin));`
`​                close(in_fd);`
`​            }`
`​            execv(get_path(command), parameters);`
`​        }`
`​        tokens_destroy(tokens);    
​    }`

`***释放参数***`

​    `free(parameters);`
`​    free(buffer);`
`​    return 0;`
`}``