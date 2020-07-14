#include <stdio.h>
#include <unistd.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <math.h>

#ifndef STDIN_FILENO
#define STDIN_FILENO fileno(stdin)
#define STDOUT_FILENO fileno(stdout)
#endif

#undef BUFSIZ
#define BUFSIZ 512

#define INPUT 0
#define OUTPUT 1

#define HIST 1
#define DEBUG 0

#define HISTORY ".my_history"

typedef struct {
    int ik;
    char** dar;
}allocsplit_t;

typedef struct {
    char** arg;
    char** in;
    char** out;
    int args;
    int ins;
    int outs;
    int commands;
}command_t;

void myHISTORY (command_t command);
command_t* setHISTORY (command_t* command, char* buf, int cnt);
void myPIPE(command_t command, int in, int out, int openfd[1024][2], int pipeNum);
void myECHO(command_t command);
int myEXIT(command_t command);
void myCD(command_t command);
void trim(char **s);
int split(int n, int w, char **dar, char *src, int del);
command_t shellsplit(char* commandstr);
allocsplit_t allocsplit(char *src, char *reg_del);
command_t* shellsplitEX(char* commandstr);

int main() {
  pid_t pid;
  command_t* command;
  int i,j,fd[1024][2],pipeNum=0;
  int status,cnt=1;
  char buf[BUFSIZ];
  char s[100];
  for(;;) {
    
    printf("shell@%s: ",getcwd(s,100));
    if(fgets (buf, sizeof(buf), stdin) == NULL) break;
    command = shellsplitEX(buf);
#if HIST
    command = setHISTORY(command, buf, cnt);
#endif
    char *p,*q,*r;
    for(i = 0; i < command->commands; ++i) {
        p = command[i].arg[command[i].args-1];
        command[i].arg[command[i].args] = NULL;
        while(*p != '\0') {
            if (*p == '\n') *p = '\0';
            p++;
        }
        if(command[i].ins != 0) {
            q = command[i].in[command[i].ins-1];
            command[i].out[command[i].ins] = NULL;
            while(*q != '\0') {
                if (*q == '\n') *q = '\0';
                q++;
            }
        }
        if(command[i].outs != 0) {
            r = command[i].out[command[i].outs-1];
            command[i].in[command[i].outs] = NULL;
            while(*r != '\0') {
                if (*r == '\n') *r = '\0';
                r++;
            }
        }
    }
#if DEBUG
    printf("cmds:%d\n",command->commands);
    for(int j = 0; j < command->commands; ++j) {
        for(i = 0; i < command[i].args; ++i) {
            printf("arg[%d]:%s\n",i,command[j].arg[i]);
        }
        for(i = 0; i < command[i].ins; ++i) {
            printf("in[%d]:%s\n",i,command[j].in[i]);
        }
        for(i = 0; i < command[i].outs; ++i) {
            printf("out[%d]:%s\n",i,command[j].out[i]);
        }
    }
#endif
    if(strcmp(command->arg[0],"myexit") == 0) {
        exit(myEXIT(command[0]));
    }
    if(strcmp(command->arg[0],"mycd") == 0) {
        myCD(command[0]);
    }

    pid=fork();
    if(pid==0){
        pipeNum = command->commands-1;
        for(j = 0; j < pipeNum; ++j) pipe(fd[j]);
        i = pipeNum-1;
        if (pipeNum > 0) {
            myPIPE(command[i+1], fd[i][INPUT], STDOUT_FILENO, fd, pipeNum);
            for(; i > 0; --i) {
                myPIPE(command[i], fd[i-1][INPUT], fd[i][OUTPUT], fd, pipeNum);
            }
            myPIPE(command[i], STDIN_FILENO, fd[i][OUTPUT], fd, pipeNum);
        } else {
            myPIPE(command[0], STDIN_FILENO, STDOUT_FILENO, fd, pipeNum);
        }
        for(j = 0; j < pipeNum; j++){
            close(fd[j][0]);close(fd[j][1]);
    	}
    }
    while (wait (&status)!=-1);
    cnt++;
  }
  return 0;
}

void myPIPE(command_t command, int in, int out, int openfd[1024][2], int pipeNum) {
    int i;
    if(pipeNum == 0) {
	    for(i = 0; i < command.ins; ++i) in = open(command.in[i], O_RDONLY, 0644);
        for(i = 0; i < command.outs; ++i) {
            if(strcmp(command.out[i], HISTORY) == 0) {
                out = open(command.out[i], O_CREAT | O_WRONLY | O_TRUNC | O_APPEND, 0644);
            } else {
                out = open(command.out[i], O_CREAT | O_WRONLY | O_TRUNC, 0644);
            }
        }
        dup2(in, STDIN_FILENO);
        dup2(out, STDOUT_FILENO);
        for(int i = 0; i < pipeNum; ++i) {
            close(openfd[i][INPUT]);close(openfd[i][OUTPUT]);
        }
        if(strcmp(command.arg[0], "myecho") == 0) {
            myECHO(command);exit(1);
        }
        if(strcmp(command.arg[0], "myexit") == 0) {
            exit(myEXIT(command));
        }
#if HIST
        if(strcmp(command.arg[0], "myhist") == 0) {
            myHISTORY(command);exit(1);
        }
#endif
        if(strcmp(command.arg[0], "mycd") == 0) {
            myCD(command);exit(1);
        }
        execvp(command.arg[0], command.arg);
	    perror("execvp");exit(1);
    }
    pid_t child;
    child = fork();

    if(child == 0) {
	    for(i = 0; i < command.ins; ++i) in = open(command.in[i], O_RDONLY, 0644);
        for(i = 0; i < command.outs; ++i) {
            if(strcmp(command.out[i], HISTORY) == 0) {
                out = open(command.out[i], O_CREAT | O_WRONLY | O_TRUNC | O_APPEND, 0644);
            } else {
                out = open(command.out[i], O_CREAT | O_WRONLY | O_TRUNC, 0644);
            }
        }
        dup2(in, STDIN_FILENO);
        dup2(out, STDOUT_FILENO);
        for(int i = 0; i < pipeNum; ++i) {
            close(openfd[i][INPUT]);close(openfd[i][OUTPUT]);
        }
        if(strcmp(command.arg[0], "myecho") == 0) {
            myECHO(command);exit(1);
        }
        if(strcmp(command.arg[0], "myexit") == 0) {
            exit(myEXIT(command));
        }
#if HIST
        if(strcmp(command.arg[0], "myhist") == 0) {
            myHISTORY(command);exit(1);
        }
#endif
        if(strcmp(command.arg[0], "mycd") == 0) {
            myCD(command);exit(1);
        }
        execvp(command.arg[0], command.arg);
        perror("execvp");exit(1);
    }
}

void myHISTORY(command_t command) {
    int fd,i = 0;
    struct stat fileStat;
    fd = open(HISTORY,O_RDONLY,0644);
    fstat(fd, &fileStat);
    char* buff = (char*)malloc(fileStat.st_size+1);
    memset(buff,0x0,fileStat.st_size+1);
    if(-1 == fd) {
        perror("History Not Found");
        exit(1);
    }
    read(fd,buff,fileStat.st_size);
    while ( buff[i] != 0 && i < BUFSIZ) {
        printf("%c",buff[i]);
        i++;
    }
    free(buff);close(fd);
}


command_t* setHISTORY (command_t* command, char* buf, int cnt) {
    char s[BUFSIZ];
    snprintf(s, BUFSIZ, "%d  %s", cnt, buf);
    int fd=open(HISTORY,O_WRONLY | O_APPEND | O_CREAT ,0644);
    write(fd,s,strlen(s));
    close(fd);
    return command;
}

void myECHO(command_t command) {
    int i = 1,flg = 0;
    if(strcmp(command.arg[1],"-n") == 0) {
        flg = 1;
        i++;
    }
    if(command.args > 0) {
        for(; i < command.args; ++i) {
            if(i != command.args-1) printf("%s ",command.arg[i]);
        }
        printf("%s",command.arg[i-1]);
    }
    if(flg == 0) printf("\n");
    exit(1);
}

int myEXIT(command_t command) {
    int i, loop, num = 0;
    char* tmp = command.arg[1];
    if(command.args == 1) return 0;
    loop = strlen(tmp);
    for(i = 0; i < loop; ++i) {
        if(tmp[loop-i-1]-'0' > 9) {
            printf("%s: %s: numeric argument required\n",command.arg[0], command.arg[1]);
            return 1;
        }
        num += (int)((tmp[loop-i-1] - '0') * pow(10,i));
    }
    return num%256;
}

void myCD(command_t command) {
    chdir(command.arg[1]);
}

void
trim(char **s)
{
    int i;
    for(i = 0; (*s)[i] != '\0'; ++i);
    for(; (*s)[i] == ' ' || (*s)[i] == '\t' || (*s)[i] == '\0'; --i);
    (*s)[i+1] = '\0';
    for(i = 0; (*s)[i] == ' ' || (*s)[i] == '\t'; ++i);
    (*s)+=i;
}

int
split(int n, int w, char **dar, char *src, int del)
{     
    int i,j,k;
    if (src == NULL) return -1;
    for ( i = 0,j = 0,k = 0; src[k] != '\0'; ++j,++k) {
        if (i >= n || j >= w) {
            i = -1;
            goto size_err;
        }
        if (k == 0 && src[k] == del) { 
            j = -1;
            continue;
        }
        if (src[k] == del) {
            if (src[k+1] == '\0') break;
            dar[i++][j] = '\0';
            j = -1;
        }
        else dar[i][j] = src[k];
    }
    dar[i++][j] = '\0';
    
    for(j = 0; j < i; ++j) trim(&dar[j]);
    
    size_err:
    return i;
}

allocsplit_t
allocsplit(char *src, char *reg_del)
{
    regex_t reg_buf;
    regmatch_t *match_buf;
    size_t size;
    int i,j,k,idx=0;
    const int n = 10;
    const int w = BUFSIZ;
    allocsplit_t as;
    as.ik=-1;
    
    as.dar = (char**)malloc(sizeof(char*)*w);
    if(!as.dar) {
        exit(3);
    }
    for(i = 0; i < n; ++i) {
        as.dar[i] = (char*)malloc(sizeof(char)*w);
        if( ! as.dar[i] ) {
            exit(5);
        }
    }

    if (regcomp(&reg_buf, reg_del, REG_EXTENDED | REG_NEWLINE) != 0) {
        puts("regex compile failsed");
	return as;
    }
    
    size = reg_buf.re_nsub+1;
    match_buf = malloc(sizeof(regmatch_t)*size);
    if(match_buf == NULL) {
        printf("no memory\n");
	idx = -1;
        goto err_reg;
    }
    
    if (regexec(&reg_buf, src, size, match_buf, 0) != 0) {
	match_buf[0].rm_so=-1;
	match_buf[0].rm_eo=-1;
    }
    


    for(j = 0,k = 0; src[j] != '\0';++j,++k) {
        if(idx >= n || k >= w) {
	    goto err_match;
	}
	as.dar[idx][k] = src[j];
	if(0 == match_buf[0].rm_so && j == 0) { 
	    j = match_buf[0].rm_eo-1;
	    k = -1;
	    if(regexec(&reg_buf, &src[j+1], size, match_buf, 0) != 0) {
                continue;
            }else {
		match_buf[0].rm_so += j+1;
                match_buf[0].rm_eo += j+1;
	    }
	    continue;
	}
        if(j == match_buf[0].rm_so) {
	    
	    if('\0' == src[match_buf[0].rm_eo]) break;
            
	    j = match_buf[0].rm_eo-1;
            as.dar[idx][k] = '\0';
            idx++;
            k = -1;
	    if(regexec(&reg_buf, &src[j+1], size, match_buf, 0) != 0) {
                continue;
            }else {
		match_buf[0].rm_so += j+1;
                match_buf[0].rm_eo += j+1;
	    }
        }
    }
    as.dar[idx++][k] = '\0';
    as.ik = idx;
    err_match:
        free(match_buf);
    err_reg:
        regfree(&reg_buf);
    return as;
}

command_t
shellsplit(char* commandstr){

  command_t command;
  allocsplit_t as;
  int i,idx;

  as = allocsplit(commandstr, ">");
  command.out = (char**)malloc(sizeof(char*)*as.ik);
  command.outs = 0;
  for(i = 1; i < as.ik; ++i){
      command.out[i-1] = allocsplit(as.dar[i], "[ \t><]+").dar[0];
      command.outs = i;
  }

  as = allocsplit(commandstr, "<");
  command.in = (char**)malloc(sizeof(char*)*as.ik);
  command.ins = 0;
  for(i = 1; i < as.ik; ++i){
      command.in[i-1] = allocsplit(as.dar[i], "[ \t><]+").dar[0];
      command.ins = i;
  }


  idx = 0;
  as = allocsplit(commandstr, "[ \t]+");
  command.arg = (char**)malloc(sizeof(char*)*as.ik);
  command.args = 0;
  for(i = 0;i < as.ik; ++i){
    if(as.dar[i][0] == '>' || as.dar[i][0] == '<'){
       if(as.dar[i][1] == '\0') i++;
        continue;
    }
    command.arg[idx++] = as.dar[i];
    command.args = idx;
  }

  return command;
}

command_t*
shellsplitEX(char* commandstr){
    int i,ik;
    const int n = BUFSIZ;
    const int w = BUFSIZ;
    char** chunk = (char**)malloc(sizeof(char*)*n);
    if(!chunk) {
        exit(3);
    }
    for(i = 0; i < n; ++i) {
        chunk[i] = (char*)malloc(sizeof(char)*w);
        if( ! chunk[i] ) {
            exit(5);
        }
    }
    ik = split(n, w, chunk, commandstr, '|'); 
    command_t* command = (command_t*)malloc(sizeof(command_t)*(ik));
    for(i = 0; i < ik; ++i) {
        command[i] = shellsplit(chunk[i]);
    }
    command->commands = ik;
    return command;
}
