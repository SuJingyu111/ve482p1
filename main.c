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
#define MAXDIRLENGTH 100 // maximum current directory length
#define MAXJOB 128 // maximum #job

enum{ROUT, ROUTA, RIN}; // >, >>, <

struct redirectsymbol{
    int pos; // after which argument in this command
    int symbol; // >, >>, <
};

enum{FG, BG};

struct job{
    int jobid;
    pid_t pid[MAXPIPELINE];
    int numpid;
    int forb;
    char content[MAXCHAC+1];
};

struct job jobgroup[MAXJOB];
int numjob = 0;

struct cmdinfo{
    int cmdpos; // the end position of this command in **arg
    int cmdnumrd; // #redirection before the end of this command
};

char ** parse(char *line, char ** arg, int *numarg, int *numrd, struct  redirectsymbol *rd,
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
        if(line[i] == '"') {
            i++;
            while(line[i] != '"') {
                if(line[i] == '\\'){
                    if(line[i+1] == '\'' || line[i+1] == '\"' || line[i+1] == '$' || line[i+1] == '\\'){
                        i++;
                    }
                }
                if(line[i] == '\n'){
                    arg[*numarg][j]=line[i];
                    j++;
                    i = 0;
                    while(fgets(line, MAXCHAC+1, stdin) == NULL){} // must be wrong!!!!!!!!!!!!!!!!!!!!!!!!
                    continue;
                }
                arg[*numarg][j]=line[i];
                j++;
                i++;
            }
            i++; //skip the last "
            arg[*numarg][j] = '\0';
            (*numarg)++;
            arg = (char **) realloc(arg, ((*numarg)+1) * sizeof(char *));
            arg[(*numarg)] = (char *)calloc(MAXCINW, sizeof(char));
            continue;
        }
        if(line[i] == '\'') {
            i++;
            while(line[i] != '\'') {
                if(line[i] == '\n'){
                    arg[*numarg][j]=line[i];
                    j++;
                    i = 0;
                    fgets(line, MAXCHAC+1, stdin);
                    continue;
                }
                arg[*numarg][j]=line[i];
                j++;
                i++;
            }
            i++; //skip the last '
            arg[*numarg][j] = '\0';
            (*numarg)++;
            arg = (char **) realloc(arg, ((*numarg)+1) * sizeof(char *));
            arg[(*numarg)] = (char *)calloc(MAXCINW, sizeof(char));
            continue;
        }

        while(line[i] != ' ' && line[i] != '>' && line[i] != '<'
            && line[i] != '|' && line[i] != '\'' && line[i] != '\"' && line[i] != '\n'){
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

int execbuiltin(char **cmd, int numarg){
    if(strcmp(cmd[0], "pwd")==0){
        char dir[MAXDIRLENGTH];
        if(getcwd(dir, sizeof(dir)) == NULL){
            printf("Error in getting current directory.\n");
            fflush(stdout);
            return 1;
        }
        printf("%s\n", dir);
        fflush(stdout);
        return 0;
    }
    else if(strcmp(cmd[0], "cd") == 0){
        if(numarg > 2){
            fprintf(stderr, "Too many arguments for cd.\n");
        }
        else if(numarg < 2){
            fprintf(stderr, "Too few arguments for cd.\n");
            fflush(stdout);
        }
        else{
            if(chdir(cmd[1]) < 0){
                fprintf(stderr, "Cannot change to directory \"%s\".\n", cmd[1]);
                fflush(stdout);
            }
        }
    }

    return 0;
}

int execwithrd(char **arg, char **command, int numarg, int numrd, struct redirectsymbol *rd){
    struct redirectsymbol last;
    last.symbol = 0; // default
    last.pos = numarg;
    rd[numrd] = last;

    int numcom=0;
    int countarg=0;
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

    // built-in
    if(strcmp(command[0], "pwd") == 0 || strcmp(command[0], "cd") == 0) {
        execbuiltin(command, numcom);
        //execbuiltin(command);
        //free(command);
        exit(0);
    }
    else{
        if(execvp(command[0], command)<0){
            printf("Cannot execute \"%s\"!\n", command[0]);
            fflush(stdout);
            //free(command);
            exit(0);
        }
    }

    //free(command);

    return 0;
}

int runsinglecmd(char **arg, int numarg, int numrd, struct redirectsymbol *rd, struct job *temp)
{
    char **command = (char **)malloc((numarg-numrd+1) * sizeof(char *));
    pid_t child;
    child = fork();
    if(child == -1) {
        printf("Failed to fork.\n");
        fflush(stdout);
        return 1;
    }
    if(child != 0){
        temp->pid[0] = child;
        temp->numpid = 1;
        jobgroup[numjob] = *temp;

        int status;
        waitpid(child, &status, 0);

    }
    else{
        if(numrd == 0){
            if(strcmp(arg[0], "pwd") == 0 || strcmp(arg[0], "cd") == 0){
                execbuiltin(arg,numarg);
                //execbuiltin(arg);
                free(command);
                exit(0);
            } // builtin
            else{
                if(execvp(arg[0], arg)<0){
                    printf("Cannot execute \"%s\"!\n", arg[0]);
                    fflush(stdout);
                    exit(0);
                }
            }
        }
        else{
            execwithrd(arg, command, numarg, numrd, rd);
        }
    }
    free(command);
    return 0;
}

int execwithpipe_helper(char **arg, int totalnumcm, int currentnumcm,
                        struct redirectsymbol *rd, struct cmdinfo *cmd, struct job *temp)
{
    if(currentnumcm > totalnumcm) return 0;

    char **newarg;
    struct redirectsymbol newrd[MAXREDIRECTION];
    int cntarg = 0;
    int cntrd = 0;
    if(currentnumcm == 0){
        newarg =  (char **)malloc((cmd[currentnumcm].cmdpos+1) * sizeof(char *));
        for(int i = 0; i < cmd[currentnumcm].cmdpos; i++){
            newarg[cntarg++] = arg[i];
        }
        for(int i = 0; i < cmd[currentnumcm].cmdnumrd; i++){
            newrd[cntrd++] = rd[i];
        }
    }
    else{
        newarg = (char **)malloc((cmd[currentnumcm].cmdpos-cmd[currentnumcm-1].cmdpos+1) * sizeof(char *));
        for(int i = cmd[currentnumcm-1].cmdpos; i < cmd[currentnumcm].cmdpos; i++){
            newarg[cntarg++] = arg[i];
        }
        for(int i = cmd[currentnumcm-1].cmdnumrd; i < cmd[currentnumcm].cmdnumrd; i++){
            newrd[cntrd++] = rd[i];
        }
    }
    newarg[cntarg] = NULL;

    int fds[2];
    if(totalnumcm != currentnumcm){
        if(pipe(fds) == -1){
            printf("Failed to pipe.\n");
            fflush(stdout);
            return 1;
        }
    }

    char **command = (char **)malloc((cntarg-cntrd+1) * sizeof(char *));
    pid_t pid = vfork();
    if(pid == -1) {
        printf("Failed to fork\n");
        fflush(stdout);
        free(newarg);
        return 1;
    }
    if(pid == 0){
        if(totalnumcm != currentnumcm){
            close(fds[0]);
            dup2(fds[1], STDOUT_FILENO);
            close(fds[1]);
        }
        if(cntrd == 0){
            if(strcmp(newarg[0], "pwd") == 0 || strcmp(newarg[0], "cd") == 0){
                execbuiltin(newarg, cntarg);
                //execbuiltin(arg);
                exit(0);
            } // builtin
            else{
                if(execvp(newarg[0], newarg)<0){
                    printf("Cannot execute \"%s\"!\n", newarg[0]);
                    fflush(stdout);
                    exit(0);
                }
            }
        }
        else{
            execwithrd(newarg, command, cntarg, cntrd, newrd);
        }
    }
    else{
        temp->pid[currentnumcm] = pid;
        jobgroup[numjob] = *temp;

        if(totalnumcm != currentnumcm){
            close(fds[1]);
            dup2(fds[0], STDIN_FILENO);
            close(fds[0]);
        }
        execwithpipe_helper(arg, totalnumcm, currentnumcm+1, rd, cmd, temp);

        int status;
        waitpid(pid, &status, 0);

    }
    free(command);
    free(newarg);

    return 0;
}

/*int execwithpipe_helper(char **arg, int totalnumcm,
                      struct redirectsymbol *rd, struct cmdinfo *cmd, struct job *temp)
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
                if(strcmp(newarg[0], "pwd") == 0 || strcmp(newarg[0], "cd") == 0){
                    execbuiltin(newarg, cntarg);
                    //execbuiltin(arg);
                    exit(0);
                } // builtin
                else{
                    if(execvp(newarg[0], newarg)<0){
                        printf("Cannot execute \"%s\"!\n", newarg[0]);
                        fflush(stdout);
                        exit(0);
                    }
                }
            }
            else{
                execwithrd(newarg, cntarg, cntrd, newrd);
            }
        }
        else{
            temp->pid[count] = pid;
            jobgroup[numjob] = *temp;

            int status;
            waitpid(pid, &status, 0);

            if(count != totalnumcm){
                close(fds[1]);
                dup2(fds[0], STDIN_FILENO);
                close(fds[0]);
            }


        }

        count++;
        free(newarg);
    }

    temp->numpid = count;
    jobgroup[numjob] = *temp;

    return 0;
}*/


int execwithpipe(char **arg, int totalnumarg, int totalnumrd, int totalnumcm,
        struct redirectsymbol *rd, struct cmdinfo *cmd, struct job *temp)
{

    struct cmdinfo last;
    last.cmdnumrd = totalnumrd;
    last.cmdpos = totalnumarg;
    cmd[totalnumcm] = last;

    temp->numpid = totalnumcm+1;
    jobgroup[numjob] = *temp;

    if(totalnumcm == 0) return runsinglecmd(arg, totalnumarg, totalnumrd, rd, temp);

    int recordin = dup(STDIN_FILENO);
    int recordout = dup(STDOUT_FILENO);
    execwithpipe_helper(arg, totalnumcm, 0, rd, cmd, temp);
    dup2(recordin, STDIN_FILENO);
    dup2(recordout, STDOUT_FILENO);

    return 0;
}

void init(struct job *jobgroup)
{
    for(int i = 0; i < MAXJOB; i++){
        for(int ii = 0; ii < MAXPIPELINE; ii++){
            jobgroup[i].pid[ii] = 0;
        }
        jobgroup[i].numpid = 0; // default
        jobgroup[i].jobid = i+1; // default
        jobgroup[i].forb = BG; // default
        jobgroup[i].content[0] = '\0'; // default
    }
}

/*void printjob(int numjob, struct job *jobgroup){
    for(int i = 0; i < numjob; i++){
        printf("[%d]", jobgroup[i].jobid);
        for(int ii = 0; ii < jobgroup[i].numpid; ii++){
            printf(" (%d)", jobgroup[i].pid[ii]);
        }
        printf(" %s\n", jobgroup[i].content);
    }
}*/

void sigint_handler()
{
    //printjob(MAXJOB, jobgroup);
    for(int i = 0; i < MAXJOB; i++){
        if(jobgroup[i].forb == FG){
            for(int ii = 0; ii < jobgroup[i].numpid; ii++){
                if(jobgroup[i].pid[ii] != 0 && kill(jobgroup[i].pid[ii], 0) == 0){
                    kill(-jobgroup[i].pid[ii], SIGINT);
                    printf("\n");
                    return;
                }
            }
        }
    }
    printf("\nmumsh $ ");
    fflush(stdout);
}

int main() {

    char line[MAXCHAC+1]; //input; +1 to ensure the last element of line is '\0'

    init(jobgroup);

    signal(SIGINT, sigint_handler);

    while(1){

        printf("mumsh $ ");
        fflush(stdout);

        if(fgets(line, MAXCHAC+1, stdin) == NULL){
            break; // ctrl+d is pressed
        }
        fflush(stdin);

        if(strncmp(line, "exit", 4) == 0 && line[4] == '\n'){
            //printjob(numjob, jobgroup);
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

        struct job temp;
        for(int i = 0; i < MAXCHAC+1; i++){
            temp.content[i] = line[i];
        }
        temp.jobid = numjob+1;
        if(line[strlen(line)-1] == '&'){
            temp.forb = BG;
        }
        else{
            temp.forb = FG;
        }
        temp.numpid = 0; // default


        arg = parse(line, arg, &numarg, &numrd, rd, &numcm, cmd);
        // if it is a background process, remember to remove '&' !!!!!!!!!!!!!!!!!!!!!!!!
        free(arg[numarg]);
        arg[numarg] = NULL;

        if(strcmp(arg[0], "cd") == 0 && numcm == 0 && numrd == 0){
            if(numarg > 2){
                printf("Too many arguments for cd.\n");
                fflush(stdout);
            }
            else if(numarg < 2){
                printf("Too few arguments for cd.\n");
                fflush(stdout);
            }
            else{
                if(chdir(arg[1]) < 0){
                    printf("Cannot change to directory \"%s\".\n", arg[1]);
                    fflush(stdout);
                }
            }
        }
        else{
            execwithpipe(arg, numarg, numrd, numcm, rd, cmd, &temp);
            jobgroup[numjob] = temp;
            numjob = (numjob+1) % MAXJOB;
        }

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
