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
#define MAXREDIRECTION 128 // maximum #redirection

enum{ROUT, ROUTA, RIN}; // >, >>, <

struct redirectsymbol{
    int pos; // after which argument
    int symbol; // >, >>, <
};

char ** parse(const char *line, char ** arg, int *numarg, int *numrd, struct  redirectsymbol *rd, int *numcm, int *cmpos)
// REQUIRES: *numcm = *numarg = 0
// EFFECTS: separate line into arg[] according to white space;
//          numcm records the number of command;
//          cmpos[a] records which argument the ath command is up to,
//          i. e. command a is arg[cmpos[a-1]] ~ arg[cmpos[a]-1].
//          return arg.
//NO REDIRECTION & NO CONSIDERING INCOMPLETE QUOTES
{

    //cmpos[0] = -1;

    int i=0;
    while(i<MAXCHAC+1){

        while(line[i] == ' ') i++;

        if(line[i] == '\n') {
            break;
        }

        /*if(line[i] == '|'){
            (*numcm)++;
            cmpos[(*numcm)] = *numarg;
            continue;
        }*/

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

        while(line[i] != ' ' && line[i] != '>' && line[i] != '<' && line[i] != '\n'){
            arg[*numarg][j]=line[i];
            j++;
            i++;
        }
        arg[*numarg][j] = '\0';

        while(line[i] == ' ') i++;

        if(line[i] == '>'){
            if(i<(MAXCHAC+2) && line[i+1]=='>'){
                struct redirectsymbol temp;
                temp.pos=(*numarg);
                temp.symbol=ROUTA;
                rd[*numrd] = temp;
                (*numrd)++;
                i++;
            }
            else{
                struct redirectsymbol temp;
                temp.pos=(*numarg);
                temp.symbol=ROUT;
                rd[*numrd] = temp;
                (*numrd)++;
            }
            i++;
        }
        else if(line[i] == '<'){
            struct redirectsymbol temp;
            temp.pos=(*numarg);
            temp.symbol=RIN;
            rd[*numrd] = temp;
            (*numrd)++;
            i++;
        }

        (*numarg)++;
        arg = (char **) realloc(arg, ((*numarg)+1) * sizeof(char *));
        arg[(*numarg)] = (char *)calloc(MAXCINW, sizeof(char));

    }

    return arg;

}


/*int runsimplecommand(char **arg)
{
    pid_t child;
    int status;
    child = fork();
    if(child == -1) {
        printf("Failed to fork.\n");
        fflush(stdout);
        return 1;
    }
    if(child != 0){
        wait(&status);
    }
    else{
        if(execvp(arg[0], arg)<0){
            printf("Cannot execute \"%s\"!\n", arg[0]);
            fflush(stdout);
            exit(0);
        }
    }
    return 0;
}*/

int runinoutcommand(char **arg, int numrd, struct redirectsymbol *rd)
{
    pid_t child;
    int status;
    child = fork();
    if(child == -1) {
        printf("Failed to fork.\n");
        fflush(stdout);
        return 1;
    }
    if(child != 0){
        wait(&status);
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
            for(int i = 0; i < numrd-1; i++){
                if(rd[i].pos >= rd[i+1].pos-1 && arg[rd[i+1].pos][0] == '\0'){
                    printf("Lack file to redirect.\n");
                    fflush(stdout);
                    return 1;
                }
                int fp[rd[i+1].pos-rd[i].pos];
                if(rd[i].symbol == ROUT){
                    int numfp=0;
                    for(int pos = rd[i].pos+1; pos < rd[i+1].pos; pos++){
                        fp[numfp] = open(arg[pos], O_CREAT|O_TRUNC);
                        if(fp[numfp] < 0){
                            printf("Cannot open file %s", arg[pos]);
                            fflush(stdout);
                            return 1;
                        }
                        int fd = dup2(fp[numfp], STDOUT_FILENO);
                        if(fd<0){
                            printf("Cannot duplicat file descriptor.\n");
                            fflush(stdout);
                        }
                        numfp++;
                    }
                }
                else if(rd[i].symbol == ROUTA){
                    int numfp=0;
                    for(int pos = rd[i].pos+1; pos < rd[i+1].pos; pos++){
                        fp[numfp] = open(arg[pos], O_CREAT|O_APPEND);
                        if(fp[numfp] < 0){
                            printf("Cannot open file %s", arg[pos]);
                            fflush(stdout);
                            return 1;
                        }
                        int fd = dup2(fp[numfp], STDOUT_FILENO);
                        if(fd<0){
                            printf("Cannot duplicat file descriptor.\n");
                            fflush(stdout);
                        }
                        numfp++;
                    }
                }
                else{
                    int numfp=0;
                    for(int pos = rd[i].pos+1; pos < rd[i+1].pos; pos++){
                        //fp[numfp] = open(arg[pos], O_CREAT|O_APPEND);改
                        if(fp[numfp] < 0){
                            printf("Cannot open file %s", arg[pos]);
                            fflush(stdout);
                            return 1;
                        }
                        //int fd = dup2(fp[numfp], STDOUT_FILENO);改
                        if(fd<0){
                            printf("Cannot duplicat file descriptor.\n");
                            fflush(stdout);
                        }
                        numfp++;
                    }
                }
            }
            //最后一个rd处理
        }
    }
    return 0;
}





int main() {

    char line[MAXCHAC+1]; //input; +1 to ensure the last element of line is '\0

    while(1){

        printf("mumsh $ ");
        fflush(stdout);

        if(fgets(line, MAXCHAC+1, stdin) == NULL){
            printf("Error in getting command.\n");
            fflush(stdout);
            break;
        }

        if(strncmp(line, "exit", 4) == 0 && line[4] == '\n') break;

        if(line[0] == '\n') continue;


        int numarg = 0, numcm = 0;
        char **arg;
        arg = (char **)malloc(sizeof(char *)); //first assign one argument
        arg[0] = (char *)calloc(MAXCINW, sizeof(char)); //suppose #characters of a word will not exceed MAXCINW
        int cmpos[MAXPIPELINE]; //suppose there are at most MAXPIPELINE pipelines
        int numrd = 0;
        struct redirectsymbol rd[MAXREDIRECTION];

        arg = parse(line, arg, &numarg, &numrd, rd, &numcm, cmpos);
        free(arg[numarg]);
        arg[numarg] = NULL;




        //runsimplecommand(arg);



        for(int i=0; i<numarg; i++){
            free(arg[i]);
        }
        free(arg);

    }




    return 0;
}