#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAXCHAC 1024

void parse(char *line, char **arg, int *numarg, int *numcm, int *cmpos)
// REQUIRES: *numcm = *numarg = 0
// EFFECTS: separate line into arg[] according to white space, deal with quotes;
//          numcm records the number of command;
//          cmpos[a] records which argument the ath command is up to,
//          i. e. command a is arg[cmpos[a-1]] ~ arg[cmpos[a]-1].
//NO REDIRECTION & NO CONSIDERING INCOMPLETE QUOTES
{

    cmpos[0] = -1;

    for(int i=0; i<MAXCHAC; i++){

        while(line[i] == ' ') i++;

        if(line[i] == '\n') {
            arg[*numarg] = NULL;
            return;
        }

        if(line[i] == '|'){
            (*numcm)++;
            cmpos[(*numcm)] = *numarg;
            continue;
        }

        int j=0;
        if(line[i] == '"') {
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
        }

        while(line[i] != ' ' && line[i] != '\n'){
            arg[*numarg][j]=line[i];
            j++;
            i++;
        }

        numarg++;

    }
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
        char *arg[MAXCHAC/3];
        int cmpos[MAXCHAC/2];
        //parse(line, arg, &numarg, &numcm, cmpos);报错signal11 内存不对
        /*for(int i=0; i<numarg; i++){
            for(int j=0; j<MAXCHAC; j++){
                if(arg[i][j]=='\n') break;
                printf("%c", arg[i][j]);
            }
            printf("\n");
        }*/

    }




    return 0;
}