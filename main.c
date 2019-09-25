#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAXCHAC 1024 //maximum #characters in a line
#define MAXCINW 256 //maximum #characters in a word
#define MAXPIPELINE 16 // maximum #pipeline
#define MAXREDIRECTION 24 // maximum #redirection

enum{ROUT, ROUTA, RIN}; // >, >>, <

struct redirectsymbol{
    int pos; // after which argument in this command
    int symbol; // >, >>, <
};

struct cmdinfo{
    int cmdpos; // the end position of this command in **arg
    int cmdnumrd; // #redirection before the end of this command
};

char ** parse(const char *line, char ** arg, int *numarg, int *numrd, struct  redirectsymbol *rd,
        int *numcm, struct cmdinfo *cmd)
// REQUIRES: *numcm = *numarg = 0
// EFFECTS: separate line into arg[] according to white space;
//          numcm records the number of command;
//          cmpos[a] records which argument the ath command is up to,
//          i. e. command a is arg[cmpos[a-1]] ~ arg[cmpos[a]-1].
//          return arg.
//NO REDIRECTION & NO CONSIDERING INCOMPLETE QUOTES
{
    int i=0;
    int lastcmdpos = 0;
    while(i<MAXCHAC+1){

        while(line[i] == ' ') i++;

        if(line[i] == '>'){
            if(i<(MAXCHAC+2) && line[i+1]=='>'){
                struct redirectsymbol temp;
                temp.pos=(*numarg) - lastcmdpos;
                temp.symbol=ROUTA;
                rd[*numrd] = temp;
                (*numrd)++;
                i++;
            }
            else{
                struct redirectsymbol temp;
                temp.pos=(*numarg) - lastcmdpos;
                temp.symbol=ROUT;
                rd[*numrd] = temp;
                (*numrd)++;
            }
            i++;
            continue;
        }
        else if(line[i] == '<'){
            struct redirectsymbol temp;
            temp.pos=(*numarg) - lastcmdpos;
            temp.symbol=RIN;
            rd[*numrd] = temp;
            (*numrd)++;
            i++;
            continue;
        }

        while(line[i] == ' ') i++;

        if(line[i] == '|'){
            struct cmdinfo temp;
            temp.cmdpos = *numarg;
            lastcmdpos = temp.cmdpos = *numarg;
            temp.cmdnumrd = *numrd;
            cmd[*numcm] = temp;
            (*numcm)++;
            i++;
            continue;
        }

        if(line[i] == '\n') {
            break;
        }

        int j=0;
        /*if(line[i] == '"') {
            i++;
            while(line[i] != '"' && line[i] != '\n') {
                arg[*numarg][j]=line[i];
                j++;
                i++;
            }
            i++; //skip the last "
        }
        if(line[i] == '\'') {
            i++;
            while(line[i] != '\'' && line[i] != '\n') {
                arg[*numarg][j]=line[i];
                j++;
                i++;
            }
            i++; //skip the last "
        }*/

        while(line[i] != ' ' && line[i] != '>' && line[i] != '<' && line[i] != '|' && line[i] != '\n'){
            arg[*numarg][j]=line[i];
            j++;
            i++;
        }
        arg[*numarg][j] = '\0';

        (*numarg)++;
        arg = (char **) realloc(arg, ((*numarg)+1) * sizeof(char *));
        arg[(*numarg)] = (char *)calloc(MAXCINW, sizeof(char));

    }

    return arg;

}

int execwithrd(char **arg, int numarg, int numrd, struct redirectsymbol *rd){
    struct redirectsymbol last;
    last.symbol = 0; // default
    last.pos = numarg;
    rd[numrd] = last;

    int numcom=0;
    int countarg=0;
    char **command = (char **)malloc((numarg-numrd+1) * sizeof(char *));
    for(numcom=0; numcom<rd[0].pos; numcom++){
        command[numcom] = arg[countarg++];
    }

    for(int i=0; i<numrd; i++){
        if(rd[i].pos>=numarg || (rd[i].pos<numarg && arg[rd[i].pos][0] == '\0')){
            printf("Lack file to redirect.\n");
            fflush(stdout);
            return 1;
        }
        if(rd[i].symbol == ROUT){
            int fp = open(arg[rd[i].pos], O_CREAT|O_WRONLY|O_TRUNC, 0644);
            if(fp < 0){
                printf("Cannot open file %s", arg[rd[i].pos]);
                fflush(stdout);
                return 1;
            }
            int fd = dup2(fp, STDOUT_FILENO);
            if(fd<0){
                printf("Cannot duplicate file descriptor.\n");
                fflush(stdout);
            }
            close(fp);
        }
        else if(rd[i].symbol == ROUTA){
            int fp = open(arg[rd[i].pos], O_WRONLY|O_CREAT|O_APPEND, 0644);
            if(fp < 0){
                printf("Cannot open file %s", arg[rd[i].pos]);
                fflush(stdout);
                return 1;
            }
            int fd = dup2(fp, STDOUT_FILENO);
            if(fd<0){
                printf("Cannot duplicate file descriptor.\n");
                fflush(stdout);
            }
            close(fp);
        }
        else{
            int fp = open(arg[rd[i].pos], O_CREAT|O_RDONLY, 0644);
            if(fp < 0){
                printf("Cannot open file %s", arg[rd[i].pos]);
                fflush(stdout);
                return 1;
            }
            int fd = dup2(fp, STDIN_FILENO);
            if(fd<0){
                printf("Cannot duplicate file descriptor.\n");
                fflush(stdout);
            }
            close(fp);
        }
        for(int j=rd[i].pos+1; j < rd[i+1].pos; j++){
            command[numcom++] = arg[j];
        }
    }

    command[numcom] = NULL;


    if(execvp(command[0], command)<0){
        printf("Cannot execute \"%s\"!\n", command[0]);
        fflush(stdout);
        free(command);
        exit(0);
    }

    free(command);

    return 0;
}

int runsinglecmd(char **arg, int numarg, int numrd, struct redirectsymbol *rd)
{
    pid_t child;
    child = fork();
    if(child == -1) {
        printf("Failed to fork.\n");
        fflush(stdout);
        return 1;
    }
    if(child != 0){
        int status;
        waitpid(child, &status, 0);
    }
    else{
        if(numrd == 0){
            if(execvp(arg[0], arg)<0){
                printf("Cannot execute \"%s\"!\n", arg[0]);
                fflush(stdout);
                exit(0);
            }
        }
        else{
            execwithrd(arg, numarg, numrd, rd);
        }
    }
    return 0;
}

int execwithpipe_helper(char **arg, int totalnumcm,
                      struct redirectsymbol *rd, struct cmdinfo *cmd)
{
    int count = 0;

    while(count <= totalnumcm){
        char **newarg;
        struct redirectsymbol newrd[MAXREDIRECTION];
        int cntarg = 0;
        int cntrd = 0;
        if(count == 0){
            newarg =  (char **)malloc((cmd[count].cmdpos+1) * sizeof(char *));
            for(int i = 0; i < cmd[count].cmdpos; i++){
                newarg[cntarg++] = arg[i];
            }
            for(int i = 0; i < cmd[count].cmdnumrd; i++){
                newrd[cntrd++] = rd[i];
            }
        }
        else{
            newarg = (char **)malloc((cmd[count].cmdpos-cmd[count-1].cmdpos+1) * sizeof(char *));
            for(int i = cmd[count-1].cmdpos; i < cmd[count].cmdpos; i++){
                newarg[cntarg++] = arg[i];
            }
            for(int i = cmd[count-1].cmdnumrd; i < cmd[count].cmdnumrd; i++){
                newrd[cntrd++] = rd[i];
            }
        }
        newarg[cntarg] = NULL;

        int fds[2];
        if(count != totalnumcm){
            if(pipe(fds) == -1){
                printf("Failed to pipe.\n");
                fflush(stdout);
                return 1;
            }
        }
        pid_t pid = fork();
        if(pid == -1) {
            printf("Failed to fork\n");
            fflush(stdout);
            free(newarg);
            return 1;
        }

        if(pid == 0){
            if(count != totalnumcm){
                close(fds[0]);
                dup2(fds[1], STDOUT_FILENO);
                close(fds[1]);
            }
            if(cntrd == 0){
                if(execvp(newarg[0], newarg)<0){
                    printf("Cannot execute \"%s\"!\n", newarg[0]);
                    fflush(stdout);
                    exit(0);
                }
            }
            else{
                execwithrd(newarg, cntarg, cntrd, newrd);
            }
        }
        else{
            int status;
            waitpid(pid, &status, 0);

            if(totalnumcm){
                close(fds[1]);
                dup2(fds[0], STDIN_FILENO);
                close(fds[0]);
            }

        }

        count++;
        free(newarg);
    }

    return 0;
}


int execwithpipe(char **arg, int totalnumarg, int totalnumrd, int totalnumcm,
        struct redirectsymbol *rd, struct cmdinfo *cmd)
{

    struct cmdinfo last;
    last.cmdnumrd = totalnumrd;
    last.cmdpos = totalnumarg;
    cmd[totalnumcm] = last;

    if(totalnumcm == 0) return runsinglecmd(arg, totalnumarg, totalnumrd, rd);

    int recordin = dup(STDIN_FILENO);
    int recordout = dup(STDOUT_FILENO);
    execwithpipe_helper(arg, totalnumcm, rd, cmd);
    dup2(recordin, STDIN_FILENO);
    dup2(recordout, STDOUT_FILENO);

    return 0;
}





int main() {

    char line[MAXCHAC+1]; //input; +1 to ensure the last element of line is '\0'

    while(1){

        printf("mumsh $ ");
        fflush(stdout);

        if(fgets(line, MAXCHAC+1, stdin) == NULL){
            break; // ctrl+d is pressed
        }
        fflush(stdin);

        if(strncmp(line, "exit", 4) == 0 && line[4] == '\n'){
            printf("exit\n");
            fflush(stdout);
            break;
        }

        if(line[0] == '\n') continue;


        int numarg = 0;
        char **arg;
        arg = (char **)malloc(sizeof(char *)); //first assign one argument
        arg[0] = (char *)calloc(MAXCINW, sizeof(char)); //suppose #characters of a word will not exceed MAXCINW
        int numrd = 0;
        struct redirectsymbol rd[MAXREDIRECTION];
        int numcm = 0;
        struct cmdinfo cmd[MAXPIPELINE];


        arg = parse(line, arg, &numarg, &numrd, rd, &numcm, cmd);
        free(arg[numarg]);
        arg[numarg] = NULL;

        execwithpipe(arg, numarg, numrd, numcm, rd, cmd);


        /*for(int i = 0; i < numcm; i++){
            printf("pos: %d ", cmd[i].cmdpos);
            printf("rd: %d ", cmd[i].cmdnumrd);
        }*/

        /*for(int i=0; i<numrd; i++){
            printf("rd%d ", rd[i].pos);
        }*/

        /*printf("numrd%d", numrd);*/
        //printf("%d", numarg);


        //runinoutcommand(arg, numarg, numrd, rd);

        for(int i=0; i<numarg; i++){
            free(arg[i]);
        }
        free(arg);

    }

    return 0;
}
