#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <setjmp.h>

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
    int poscmd; // which command it is in
};

char *rdsymbol[3] = {">", ">>", "<"};

enum{FG, BG};

struct job{
    int jobid;
    pid_t pid[MAXPIPELINE];
    int numpid;
    int forb;
    int state[MAXPIPELINE]; // whether it is running or done
    char content[MAXCHAC+1];
};

pid_t mainpid;
jmp_buf env;
char **arg;
int numarg = 0;

struct cmdinfo{
    int cmdpos; // the end position of this command in **arg
    int cmdnumrd; // #redirection before the end of this command
};

void sigint_handler()
{
    /*if(numarg != 0){
        for(int i = 0; i < numarg; i++){
            free(arg[i]);
        }
        free(arg);
    }*/
    printf("\n");
    siglongjmp(env, 1);
}

void proc_sigint_handler()
{
    printf("\n");
}

char ** parse(char *line, char ** arg, int *numarg, int *numrd, struct  redirectsymbol *rd,
        int *numcm, struct cmdinfo *cmd, int *error)
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
            if((*numrd) > 0 && rd[(*numrd)-1].poscmd == (*numcm)
                && rd[(*numrd)-1].pos + lastcmdpos == (*numarg)){
                printf("syntax error near unexpected token \'%c\'\n", '>');
                fflush(stdout);
                *error = 1;
                break;
            }

            if(i<(MAXCHAC+2) && line[i+1]=='>'){
                struct redirectsymbol temp;
                temp.pos=(*numarg) - lastcmdpos;
                temp.symbol=ROUTA;
                temp.poscmd = (*numcm);
                rd[*numrd] = temp;
                (*numrd)++;
                i++;
            }
            else{
                struct redirectsymbol temp;
                temp.pos=(*numarg) - lastcmdpos;
                temp.symbol=ROUT;
                temp.poscmd = (*numcm);
                rd[*numrd] = temp;
                (*numrd)++;
            }
            i++;
            continue;
        }
        else if(line[i] == '<'){
            if((*numrd) > 0 && rd[(*numrd)-1].poscmd == (*numcm)
               && rd[(*numrd)-1].pos + lastcmdpos == (*numarg)){
                printf("syntax error near unexpected token \'%c\'\n", '<');
                fflush(stdout);
                *error = 1;
                break;
            }

            struct redirectsymbol temp;
            temp.pos=(*numarg) - lastcmdpos;
            temp.symbol=RIN;
            temp.poscmd = (*numcm);
            rd[*numrd] = temp;
            (*numrd)++;
            i++;
            continue;
        }

        while(line[i] == ' ') i++;

        if(line[i] == '|'){
            if((*numrd)>0 && rd[(*numrd)-1].poscmd == (*numcm)
                && rd[(*numrd)-1].pos + lastcmdpos == (*numarg)){
                printf("syntax error near unexpected token \'%c\'\n", '|');
                fflush(stdout);
                *error = 1;
                break;
            }

            struct cmdinfo temp;
            //temp.cmdpos = *numarg;
            lastcmdpos = temp.cmdpos = *numarg;
            temp.cmdnumrd = *numrd;
            cmd[*numcm] = temp;
            (*numcm)++;
            i++;
            continue;
        }

        if(line[i] == '\n') {
            int whetherbreak = 0;
            if((*numrd) > 0 && rd[(*numrd)-1].poscmd == (*numcm) && rd[(*numrd)-1].pos + lastcmdpos == (*numarg)) whetherbreak = 1;
            if((*numcm) > 0 && cmd[(*numcm)-1].cmdpos == (*numarg)){
                whetherbreak = 1;
            }
            if(whetherbreak){
                i = 0;
                printf("> ");
                fflush(stdout);
                while(fgets(line, MAXCHAC+1, stdin) == NULL){}
                continue;
            }
            else break;
        }

        int j=0;
        while(line[i] != ' ' && line[i] != '>' && line[i] != '<'
            && line[i] != '|' && line[i] != '\n'){

            if(line[i] == '\"') {
                i++;
                while(line[i] != '\"') {
                    if(line[i] == '\\'){
                        if(line[i+1] == '\'' || line[i+1] == '\"' || line[i+1] == '$' || line[i+1] == '\\'){
                            i++;
                        }
                    }
                    if(line[i] == '\n'){
                        arg[*numarg][j]=line[i];
                        j++;
                        i = 0;
                        printf("> ");
                        fflush(stdout);
                        while(fgets(line, MAXCHAC+1, stdin) == NULL){} // must be wrong!!!!!!!!!!!!!!!!!!!!!!!!
                        continue;
                    }
                    arg[*numarg][j]=line[i];
                    j++;
                    i++;
                }
                i++; //skip the last "
                /*arg[*numarg][j] = '\0';
                (*numarg)++;
                arg = (char **) realloc(arg, ((*numarg)+1) * sizeof(char *));
                arg[(*numarg)] = (char *)calloc(MAXCINW, sizeof(char));*/
                continue;
            }
            if(line[i] == '\'') {
                i++;
                while(line[i] != '\'') {
                    if(line[i] == '\n'){
                        arg[*numarg][j]=line[i];
                        j++;
                        i = 0;
                        printf("> ");
                        fflush(stdout);
                        fgets(line, MAXCHAC+1, stdin);
                        continue;
                    }
                    arg[*numarg][j]=line[i];
                    j++;
                    i++;
                }
                i++; //skip the last '
                /*arg[*numarg][j] = '\0';
                (*numarg)++;
                arg = (char **) realloc(arg, ((*numarg)+1) * sizeof(char *));
                arg[(*numarg)] = (char *)calloc(MAXCINW, sizeof(char));*/
                continue;
            }
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
            char *home = getenv("HOME");
            chdir(home);
        }
        else{
            if(chdir(cmd[1]) < 0){
                fprintf(stderr, "%s: No such file or directory\n", cmd[1]);
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

    int countin = 0;
    for(int i=0; i<numrd; i++){
        if(rd[i].symbol == ROUT){
            int fp = open(arg[rd[i].pos], O_CREAT|O_WRONLY|O_TRUNC, 0644);
            if(fp < 0){
                printf("%s: Permission denied\n", arg[rd[i].pos]);
                fflush(stdout);
                return -1;
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
                printf("%s: Permission denied\n", arg[rd[i].pos]);
                fflush(stdout);
                return -1;
            }
            int fd = dup2(fp, STDOUT_FILENO);
            if(fd<0){
                printf("Cannot duplicate file descriptor.\n");
                fflush(stdout);
            }
            close(fp);
        }
        else{
            countin++;
            int fp = open(arg[rd[i].pos], O_RDONLY, 0644);
            if(fp < 0){
                printf("%s: No such file or directory\n", arg[rd[i].pos]);
                fflush(stdout);
                return -1;
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
    if(countin > 1){
        printf("error: duplicated input redirection\n");
        fflush(stdout);
        exit(1);
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
            printf("%s: command not found\n", command[0]);
            fflush(stdout);
            //free(command);
            exit(0);
        }
    }

    //free(command);

    return countin;
}

int runsinglecmd(char **arg, int numarg, int numrd, struct redirectsymbol *rd, struct job *temp,
                 struct job *jobgroup, int numjob, char **command)
{
    int recordin = dup(STDIN_FILENO);
    int recordout = dup(STDOUT_FILENO);
    int numinrd = 0;
    pid_t child;
    child = fork();
    if(child == -1) {
        printf("Failed to fork.\n");
        fflush(stdout);
        return 1;
    }
    if(child != 0){
        if(temp->forb == BG){
            temp->pid[0] = child;
            temp->numpid = 1;
            jobgroup[numjob] = *temp;
        }

        if(temp->forb == FG){
            int status;
            waitpid(child, &status, 0);
            //waitpid(child, &temp->state[0], 0);
        }
        else{
            int status;
            waitpid(child, &status, WNOHANG|WUNTRACED);
        }
    }
    else{
        if(temp->forb == FG) setpgid(0, mainpid);
        if(numrd == 0){
            if(strcmp(arg[0], "pwd") == 0 || strcmp(arg[0], "cd") == 0){
                execbuiltin(arg,numarg);
                //execbuiltin(arg);
                //free(command);
                exit(0);
            } // builtin
            else{
                if(execvp(arg[0], arg)<0){
                    printf("%s: command not found\n", arg[0]);
                    fflush(stdout);
                    exit(0);
                }
            }
        }
        else{
            numinrd = execwithrd(arg, command, numarg, numrd, rd);
        }
    }
    dup2(recordin, STDIN_FILENO);
    dup2(recordout, STDOUT_FILENO);
    if(numinrd >= 0 && (numrd - numinrd) > 1){
        printf("error: duplicated output redirection\n");
        fflush(stdout);
    }
    return 0;
}

int execwithpipe_helper(char **arg, int totalnumcm, int currentnumcm, struct redirectsymbol *rd,
        struct cmdinfo *cmd, struct job *temp, struct job *jobgroup, int numjob)
{
    if(currentnumcm > totalnumcm) return 0;

    char **newarg;
    struct redirectsymbol newrd[MAXREDIRECTION];
    int cntarg = 0;
    int cntrd = 0;
    if(currentnumcm == 0){
        for(int i = 0; i < cmd[currentnumcm].cmdnumrd; i++){
            newrd[cntrd++] = rd[i];
        }
        if(cntrd > 1 || (cntrd == 1 && (newrd[0].symbol == ROUT || newrd[0].symbol == ROUTA))){
            printf("error: duplicated output redirection\n");
            fflush(stdout);
            return 1;
        }

        newarg =  (char **)malloc((cmd[currentnumcm].cmdpos+1) * sizeof(char *));
        for(int i = 0; i < cmd[currentnumcm].cmdpos; i++){
            newarg[cntarg++] = arg[i];
        }
    }
    else{
        for(int i = cmd[currentnumcm-1].cmdnumrd; i < cmd[currentnumcm].cmdnumrd; i++){
            newrd[cntrd++] = rd[i];
        }
        if(currentnumcm == totalnumcm){
            if(cntrd > 1 || (cntrd == 1 && newrd[0].symbol == RIN)){
                printf("error: duplicated input redirection\n");
                fflush(stdout);
                return 1;
            }
        }

        if(cmd[currentnumcm].cmdpos == cmd[currentnumcm-1].cmdpos){
            printf("error: missing program\n");
            fflush(stdout);
            return 1;
        }

        newarg = (char **)malloc((cmd[currentnumcm].cmdpos-cmd[currentnumcm-1].cmdpos+1) * sizeof(char *));
        for(int i = cmd[currentnumcm-1].cmdpos; i < cmd[currentnumcm].cmdpos; i++){
            newarg[cntarg++] = arg[i];
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
        if(temp->forb == FG) setpgid(0, mainpid);
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
                    printf("%s: command not found\n", newarg[0]);
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
        signal(SIGINT, proc_sigint_handler);

        if(temp->forb == BG){
            temp->pid[currentnumcm] = pid;
            jobgroup[numjob] = *temp;
        }

        if(totalnumcm != currentnumcm){
            close(fds[1]);
            dup2(fds[0], STDIN_FILENO);
            close(fds[0]);
        }
        execwithpipe_helper(arg, totalnumcm, currentnumcm+1, rd, cmd, temp, jobgroup, numjob);

        if(temp->forb == FG){
            int status;
            waitpid(pid, &status, 0);
            //waitpid(pid, &temp->state[currentnumcm], 0);
        }
        else{
            waitpid(pid, &temp->state[currentnumcm], WNOHANG);
        }

        //printf("return code is %d\n",WEXITSTATUS(status));

    }
    free(command);
    free(newarg);

    return 0;
}

char **singlecommand;

int execwithpipe(char **arg, int totalnumarg, int totalnumrd, int totalnumcm,
        struct redirectsymbol *rd, struct cmdinfo *cmd, struct job *temp, struct job *jobgroup, int numjob)
{

    struct cmdinfo last;
    last.cmdnumrd = totalnumrd;
    last.cmdpos = totalnumarg;
    cmd[totalnumcm] = last;

    if(temp->forb == BG){
        temp->numpid = totalnumcm+1;
        jobgroup[numjob] = *temp;
    }

    if(totalnumcm == 0){
        singlecommand = (char **)malloc((totalnumarg-totalnumrd+1) * sizeof(char *));
        int result = runsinglecmd(arg, totalnumarg, totalnumrd, rd, temp, jobgroup, numjob, singlecommand);
        free(singlecommand);
        return result;
    }

    int recordin = dup(STDIN_FILENO);
    int recordout = dup(STDOUT_FILENO);
    execwithpipe_helper(arg, totalnumcm, 0, rd, cmd, temp, jobgroup, numjob);
    dup2(recordin, STDIN_FILENO);
    dup2(recordout, STDOUT_FILENO);

    return 0;
}

void initjob(struct job *tjob)
{
    for(int ii = 0; ii < MAXPIPELINE; ii++){
        tjob->pid[ii] = 0;
        tjob->state[ii] = -1;
    }
    tjob->numpid = 0; // default
    tjob->jobid = 1; // default
    tjob->forb = BG; // default
    tjob->content[0] = '\0'; // default
}

void init(struct job *jobgroup)
{
    for(int i = 0; i < MAXJOB; i++){
        initjob(&(jobgroup[i]));
    }
}

int isrunning(struct job tjob)
// EFFECTS: return 1 if tjob is running, return 0 if tjob is done
{
    int result = 0;
    for(int i = 0; i < tjob.numpid; i++){
        int status;
        if(waitpid(tjob.pid[i], &status, WNOHANG) == 0) {
            result = 1;
            break;
        }
        else{
            tjob.state[i] = 1;
        }
    }
    return result;
}

void printbackgroundjob(int numjob, struct job *jobgroup)
{
    for(int i = 0; i <= numjob; i++){
        if(jobgroup[i].forb == BG && jobgroup[i].pid[0] != 0){
            printf("[%d]", jobgroup[i].jobid);
            fflush(stdout);
            if(isrunning(jobgroup[i])){
                printf(" running");
            }
            else{
                printf(" done");
            }
            fflush(stdout);
            printf(" %s", jobgroup[i].content);
            fflush(stdout);
        }
    }
}

int main() {

    struct job jobgroup[MAXJOB];
    int numjob = 0;
    init(jobgroup);

    mainpid = getpid();
    setpgid(0, 0);

    signal(SIGINT, sigint_handler);
    sigsetjmp(env, 1);
    if(numarg != 0){
        for(int i = 0; i < numarg; i++){
            free(arg[i]);
        }
        free(arg);
        free(singlecommand);
    }

    while(1){

        numarg = 0;

        printf("mumsh $ ");
        fflush(stdout);

        char line[MAXCHAC+1];
        for(int i=0; i<MAXCHAC+1; i++){
            line[i] = '\0';
        }
        char tempchar;
        int cntchar = 0;
        while((tempchar = (char)fgetc(stdin)) != '\n'){
            if(tempchar == EOF){
                if(line[0] != '\0') continue;
                else{
                    printf("exit\n");
                    fflush(stdout);
                    return 0;
                }
            }
            line[cntchar] = tempchar;
            cntchar++;
        }
        line[cntchar] = '\n';

        if(strncmp(line, "exit", 4) == 0 && line[4] == '\n'){
            printf("exit\n");
            fflush(stdout);
            break;
        }

        if(strncmp(line, "jobs", 4) == 0 && line[4] == '\n'){
            printbackgroundjob(numjob, jobgroup);
            continue;
        }

        if(line[0] == '\n') continue;

        arg = (char **)malloc(sizeof(char *)); //first assign one argument
        arg[0] = (char *)calloc(MAXCINW, sizeof(char)); //suppose #characters of a word will not exceed MAXCINW
        int numrd = 0;
        struct redirectsymbol rd[MAXREDIRECTION];
        int numcm = 0;
        struct cmdinfo cmd[MAXPIPELINE];

        struct job temp;
        initjob(&temp);
        for(int i = 0; i < MAXCHAC+1; i++){
            temp.content[i] = line[i];
        }
        temp.jobid = numjob+1;
        if(line[cntchar-1] == '&'){
            line[cntchar] = '\0';
            line[cntchar-1] = '\n'; // remove '&' for background job
            temp.forb = BG;
        }
        else{
            temp.forb = FG;
        }
        temp.numpid = 0; // default

        int errorinparse = 0;
        arg = parse(line, arg, &numarg, &numrd, rd, &numcm, cmd, &errorinparse);
        free(arg[numarg]);
        arg[numarg] = NULL;

        if(!errorinparse){
            if(strcmp(arg[0], "cd") == 0 && numcm == 0 && numrd == 0){
                if(numarg > 2){
                    printf("Too many arguments for cd.\n");
                    fflush(stdout);
                }
                else if(numarg < 2){
                    char *home = getenv("HOME");
                    chdir(home);
                }
                else{
                    if(chdir(arg[1]) < 0){
                        printf("%s: No such file or directory\n", arg[1]);
                        fflush(stdout);
                    }
                }
            }
            else{
                execwithpipe(arg, numarg, numrd, numcm, rd, cmd, &temp, jobgroup, numjob);
                if(temp.forb == BG){
                    printf("[%d] ", temp.jobid);
                    fflush(stdout);
                    for(int i = 0; i<temp.numpid; i++){
                        printf("(%d) ", temp.pid[i]);
                        fflush(stdout);
                    }
                    printf("%s", temp.content);
                    fflush(stdout);
                    jobgroup[numjob] = temp;
                    numjob = (numjob+1) % MAXJOB;
                }
            }
        }

        for(int i=0; i<numarg; i++){
            free(arg[i]);
        }
        free(arg);

    }

    return 0;
}

