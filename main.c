#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAXCHAC 1024 //maximum #characters in a line
#define MAXCINW 256 //maximum #characters in a word
#define MAXPIPELINE 32 // maximum #pipeline

char ** parse(const char *line, int *numarg, int *numcm, int *cmpos)
// REQUIRES: *numcm = *numarg = 0
// EFFECTS: separate line into arg[] according to white space;
//          numcm records the number of command;
//          cmpos[a] records which argument the ath command is up to,
//          i. e. command a is arg[cmpos[a-1]] ~ arg[cmpos[a]-1].
//          return arg.
//NO REDIRECTION & NO CONSIDERING INCOMPLETE QUOTES
{

    //cmpos[0] = -1;

    char **arg;
    arg = (char **)malloc(sizeof(char *)); //first assign one argument
    arg[0] = (char *)malloc(sizeof(char) * MAXCINW); //suppose #characters of a word will not exceed MAXCINW

    int i=0;
    while(i<MAXCHAC+1){

        while(line[i] == ' ') i++;

        if(line[i] == '\n') {
            free(arg[*numarg]);
            arg[*numarg] = NULL;
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

        while(line[i] != ' ' && line[i] != '\n'){
            arg[*numarg][j]=line[i];
            j++;
            i++;
        }
        arg[*numarg][j] = '\0';

        (*numarg)++;
        arg = (char **) realloc(arg, ((*numarg)+1) * sizeof(char *));
        arg[(*numarg)] = (char *)malloc(sizeof(char) * MAXCINW);

    }

    return arg;

}

int main() {

    char line[MAXCHAC+1]; //input; +1 to ensure the last element of line is '\0

    while(1){

        printf("mumsh $ ");

        if(fgets(line, MAXCHAC+1, stdin) == NULL){
            printf("Error in getting command.\n");
            break;
        }

        if(strncmp(line, "exit", 4) == 0 && line[4] == '\n') break;

        if(line[0] == '\n') continue;

        int numarg = 0, numcm = 0;
        char **arg;
        //arg = (char **)malloc(sizeof(char *)); //first assign one argument
        //arg[0] = (char *)malloc(sizeof(char) * MAXCINW); //suppose #characters of a word will not exceed MAXCINW
        int cmpos[MAXPIPELINE]; //suppose there are at most MAXPIPELINE pipelines
        arg = parse(line, &numarg, &numcm, cmpos);

        /*for(int i=0; i<numarg; i++){
            if(arg[i]==NULL) break;
            for(int j=0; j<MAXCHAC; j++){
                if(!arg[i][j]) break;
                printf("%c", arg[i][j]);
            }
            printf("\n");
        }*/


        /*for(int i=0; i<numarg; i++){
            if(arg[i]!= NULL){
                for(int j=0; j<MAXCHAC; j++){
                    if(arg[i][j]=='\0') break;
                    printf("%c", arg[i][j]);
                }
                printf("\n");
            }
        }*/


        for(int i=0; i<numarg; i++){
            free(arg[i]);
        }
        free(arg);

    }




    return 0;
}