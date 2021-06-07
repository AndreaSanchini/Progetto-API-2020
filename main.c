#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHANGE 'c'
#define PRINT 'p'
#define DELETE 'd'
#define UNDO 'u'
#define REDO 'r'
#define QUIT 'q'

#define STR_BUF_MAX 1025

/*
 * global variables
 */
int curr_lines;
int in_history;
int history_pointer; /* index of the first undoable command */
int last_delete; /* index of the last delete command, set to 0 if it does not exists */
int must_wipe; /* used as a boolean, set to 1 when an undo command is executed */
char buffer[STR_BUF_MAX];
char cmd_buffer[STR_BUF_MAX];
char** curr_document;

typedef struct command{
    char type;
    int ind1;
    int ind2;
    int lines_reached;
    int last_delete;
    char** saved; /* contains inserted lines if type is change or a copy of the document if type is delete */
}command;

command** history;

/*
 * functions
 */
int command_handler();
command* create_command(int ind1, int ind2, char type);
char** copy_document();
char** retrieve_from_delete(command* delete);
int count_overwrite(int ind1, int ind2);
int check_sequence(int n);
void wipe_redo();
void copy_from_delete(command* delete);
void change_text(int ind1, int ind2);
void delete_text(int ind1, int ind2);
void print_text(int ind1, int ind2);
void undo_commands(int n);
void redo_commands(int n);

/*
 * wipes from history all the undone commands
 * that could be redone
 */
void wipe_redo(){
    int i, j;
    if(in_history == 0)
        return;
    for(i = history_pointer; i <= in_history - 1; i++){
        if(history[i]->type == CHANGE){
            for(j = 0; j <= history[i]->ind2 - history[i]->ind1; j++)
                free(history[i]->saved[j]);
            free(history[i]->saved);
            free(history[i]);
        }
        else if(history[i]->type == DELETE){
            free(history[i]->saved);
            free(history[i]);
        }
    }
    in_history = history_pointer;
    if(in_history == 0)
        last_delete = 0;
    else if(history[in_history - 1]->type == DELETE)
        last_delete = in_history;
    else if(history[in_history - 1]->type == CHANGE)
        last_delete = history[in_history - 1]->last_delete;
}

/*
 * handles sequences of undo and redo
 * commands
 *
 * @param   n      number of undos specified by the first undo of the sequence
 * @returns int    1 if sequence is ended by a quit command
 *                 0 otherwise
 */
int check_sequence(int n){
    int n1, n2, h1;
    char cmd;
    cmd = UNDO;
    /*
     * h1 used to simulate how history_pointer moves based
     * on the sequence of undo and redo commands
     */
    h1 = history_pointer;
    while(h1 > 0 && n > 0){
        h1 = h1 - 1;
        n = n - 1;
    }
    fgets(cmd_buffer, STR_BUF_MAX, stdin);
    cmd = cmd_buffer[strlen(cmd_buffer) - 2];
    while(cmd == UNDO || cmd == REDO || cmd == PRINT){
        if(cmd == UNDO || cmd == REDO){
            sscanf(cmd_buffer, "%d%c", &n1, &cmd);
            if(cmd == UNDO){
                while(h1 > 0 && n1 > 0){
                    h1 = h1 - 1;
                    n1 = n1 - 1;
                }
            }
            else if(cmd == REDO){
                while(h1 < in_history && n1 > 0){
                    h1 = h1 + 1;
                    n1 = n1 - 1;
                }
            }
        }
        else if(cmd == PRINT){
            if(h1 <= history_pointer)
                undo_commands(history_pointer - h1);
            if(h1 > history_pointer)
                redo_commands(h1 - history_pointer);
            sscanf(cmd_buffer, "%d,%d%c", &n1, &n2, &cmd);
            print_text(n1, n2);
        }
        fgets(cmd_buffer, STR_BUF_MAX, stdin);
        cmd = cmd_buffer[strlen(cmd_buffer) - 2];
    }
    if(cmd == QUIT)
        return 1;
    else if(h1 <= history_pointer)
        undo_commands(history_pointer - h1);
    else if(h1 > history_pointer)
        redo_commands(h1 - history_pointer);
    return 0;
}

/*
 * checks how many lines are overwritten
 *
 * @param   ind1  index of the first line that will be inserted
 * @param   ind2  index of the last line that will be inserted
 * @return  int   the amount of overwritten lines
 */
int count_overwrite(int ind1, int ind2){
    int i, overwrite;
    i = ind1;
    overwrite = 0;
    if(ind1 > curr_lines)
        return 0;
    else{
        while(i <= curr_lines && i <= ind2){
            overwrite++;
            i++;
        }
        return overwrite;
    }
}

/*
 * creates a copy of the document
 * that will be inserted in the command
 * structure in case of a delete command
 *
 * @return   char**   the created copy
 */
char** copy_document(){
    int i;
    char** tmp;
    tmp = malloc(sizeof(char*) * curr_lines);
    for(i = 0; i <= curr_lines - 1; i++)
        tmp[i] = curr_document[i];
    return tmp;
}

/*
 * retrieves a document using the
 * copy saved in a delete command
 *
 * @param   delete   source of the copy
 * @return  char**   the built document
 */
char** retrieve_from_delete(command* delete){
    int i;
    char** tmp;
    tmp = malloc(sizeof(char*) * delete->lines_reached);
    for(i = 0; i <= delete->lines_reached - 1; i++)
        tmp[i] = delete->saved[i];
    return tmp;
}

/*
 * rebuilds curr_document using
 * a previous copy saved in a
 * delete command
 *
 * @param   delete   source of the copy
 */
void copy_from_delete(command* delete){
    int i;
    for(i = 0; i <= delete->lines_reached - 1; i++)
        curr_document[i] = delete->saved[i];
}

/*
 * creates a command that will
 * be saved in history
 *
 * @param   ind1        first line of the command
 * @param   ind2        last line of the command
 * @param   type        type of the command
 * @return  command*    the created command
 */
command* create_command(int ind1, int ind2, char type){
    command* tmp;
    tmp = malloc(sizeof(command));
    tmp->ind1 = ind1;
    tmp->ind2 = ind2;
    tmp->type = type;
    tmp->last_delete = last_delete;
    if(type == CHANGE)
        tmp->saved = malloc(sizeof(char*) * (ind2 - ind1 + 1));
    else if(type == DELETE)
        tmp->saved = NULL;
    return tmp;
}

/*
 * changes the document by adding
 * or overwriting lines based
 * on the indexes provided
 *
 * @param   ind1   index of the first line that will be changed
 * @param   ind2   index of the last line that will be changed
 */
void change_text(int ind1, int ind2){
    int i, h1, overwrite;
    char* str;
    command* tmp;
    h1 = 0;
    if(must_wipe == 1){
        wipe_redo();
        must_wipe = 0;
    }
    history = realloc(history, sizeof(command*) * (in_history + 1));
    in_history = in_history + 1;
    history_pointer = history_pointer + 1;
    tmp = create_command(ind1, ind2, CHANGE);
    history[in_history - 1] = tmp;
    overwrite = count_overwrite(ind1, ind2);
    curr_document = realloc(curr_document, sizeof(char*) * (curr_lines + (ind2 - ind1 + 1) - overwrite));
    for(i = ind1; i <= ind2; i++){
        fgets(buffer, STR_BUF_MAX, stdin);
        str = malloc(sizeof(char) * (strlen(buffer) + 1));
        strcpy(str, buffer);
        if(i > curr_lines){
            curr_document[i - 1] = str;
            tmp->saved[h1] = str;
            curr_lines = curr_lines + 1;
            h1 = h1 + 1;
        }
        else{
            curr_document[i - 1] = str;
            tmp->saved[h1] = str;
            h1 = h1 + 1;
        }
    }
    tmp->lines_reached = curr_lines;
    fgetc(stdin);
    fgetc(stdin);
}

/*
 * changes the document by deleting lines
 * based on the indexes provided
 *
 * @param   ind1   index of the first line that will be deleted
 * @param   ind2   index of the last line that will be deleted
 */
void delete_text(int ind1, int ind2){
    int i, deleted;
    command* tmp;
    deleted = 0;
    if(must_wipe == 1){
        wipe_redo();
        must_wipe = 0;
    }
    history = realloc(history, sizeof(command*) * (in_history + 1));
    in_history = in_history + 1;
    history_pointer = history_pointer + 1;
    last_delete = in_history;
    tmp = create_command(ind1, ind2, DELETE);
    history[in_history - 1] = tmp;
    /*
     * used to handle corner case 0,0d
     */
    if(ind1 == 0 && ind2 == 0){
        tmp->saved = copy_document();
        tmp->lines_reached = curr_lines;
        return;
    }
    if(ind1 > curr_lines){
        tmp->saved = copy_document();
        tmp->lines_reached = curr_lines;
        return;
    }
    else{
        if(ind1 <= 0 && ind2 != 0){
            ind1 = 1;
            tmp->ind1 = 1;
        }
        for(i = ind1; i <= ind2 && i <= curr_lines; i++)
            deleted++;
        /*
         * used to shift the lines following
         * the ones which were deleted
         */
        for(i = ind1 + deleted; i <= curr_lines; i++){
            curr_document[ind1 - 1] = curr_document[i - 1];
            ind1++;
        }
        curr_lines = curr_lines - deleted;
        tmp->saved = copy_document();
        tmp->lines_reached = curr_lines;
    }
}

/*
 * prints lines of the document
 *
 * @param   ind1   index of the first line to print
 * @param   ind2   index of the last line to print
 */
void print_text(int ind1, int ind2){ // funzione per la stampa del testo
    int i;
    for(i = ind1; i <= ind2; i++){
        if(i <= 0 || i > curr_lines)
            fputs(".\n", stdout);
        else
            fputs(curr_document[i - 1], stdout);
    }
}

/*
 * undoes commands
 *
 * @param   n   number of commands to undo
 */
void undo_commands(int n){
    int i, j, k, h, ind1, ind2;
    char cmd;
    cmd = cmd_buffer[strlen(cmd_buffer) - 2];
    /*
     * if all commands are undone the document
     * is cleared
     */
    if(history_pointer - n <= 0) {
        curr_lines = 0;
        history_pointer = 0;
        free(curr_document);
        curr_document = NULL;
    }
    /*
     * if the command reached by the function is delete
     * the document is retrieved from the copy saved
     * in the command structure
     */
    else if(history_pointer - n > 0 && n != 0){
        if(history[history_pointer - n - 1]->type == DELETE){
            free(curr_document);
            curr_lines = history[history_pointer - n - 1]->lines_reached;
            curr_document = retrieve_from_delete(history[history_pointer - n - 1]);
        }
        /*
         * if the command reached by the function is a change without
         * a last_delete, the document is rebuilt from the first
         * change command
         */
        else if(history[history_pointer - n - 1]->type == CHANGE){
            if(history[history_pointer - n - 1]->last_delete == 0){
                free(curr_document);
                curr_document = malloc(sizeof(char*) * history[history_pointer - n - 1]->lines_reached);
                for(i = 0; i <= history_pointer - n - 1; i++){
                    for(j = 0, k = history[i]->ind1; j <= history[i]->ind2 - history[i]->ind1; j++, k++)
                        curr_document[k - 1] = history[i]->saved[j];
                }
            }
            /*
             * if the command reached by the function is a change with
             * a last_delete, the document is partially rebuilt from
             * the copy stored in the command structure and fully
             * reconstructed by applying all the change commands
             * following that delete
             */
            else{
                free(curr_document);
                curr_document = malloc(sizeof(char*) * history[history_pointer - n - 1]->lines_reached);
                h = history[history_pointer - n - 1]->last_delete;
                copy_from_delete(history[h - 1]);
                for(i = history[history_pointer - n - 1]->last_delete; i <= history_pointer - n - 1; i++){
                    for(j = 0, k = history[i]->ind1; j <= history[i]->ind2 - history[i]->ind1; j++, k++)
                        curr_document[k - 1] = history[i]->saved[j];
                }
            }
            curr_lines = history[history_pointer - n - 1]->lines_reached;
        }
        history_pointer = history_pointer - n;
    }
    /*
     * executes the command that followed the
     * sequence of undo and redo commands
     */
    if(cmd == CHANGE || cmd == DELETE) {
        sscanf(cmd_buffer, "%d,%d%c", &ind1, &ind2, &cmd);
        if (cmd == CHANGE)
            change_text(ind1, ind2);
        else if (cmd == DELETE)
            delete_text(ind1, ind2);
    }
}

/*
 * redoes commands
 *
 * @param   n   number of commands to redo
 */
void redo_commands(int n){
    int i, j, k, h, ind1, ind2;
    char cmd;
    /*
     * if the command reached by the function is delete
     * the document is retrieved from the copy saved
     * in the command structure
     */
    if(history[history_pointer + n - 1]->type == DELETE){
        free(curr_document);
        curr_lines = history[history_pointer + n - 1]->lines_reached;
        curr_document = retrieve_from_delete(history[history_pointer + n - 1]);
    }
    else if(history[history_pointer + n - 1]->type == CHANGE){
          curr_document = realloc(curr_document, sizeof(char *) * history[history_pointer + n - 1]->lines_reached);
          h = history[history_pointer + n - 1]->last_delete;
          /*
           * if the command reached by the function is a change without a
           * last_delete or with a last_delete smaller than history_pointer,
           * the document is fully reconstructed by applying all the change
           * commands from the one pointed by history_pointer to the last
           * that need to be redone
           */
          if (history[history_pointer + n - 1]->last_delete == 0 ||
              history[history_pointer + n - 1]->last_delete <= history_pointer){
                for (i = history_pointer + 1; i <= history_pointer + n; i++){
                    for (j = 0, k = history[i - 1]->ind1; j <= history[i - 1]->ind2 - history[i - 1]->ind1; j++, k++)
                        curr_document[k - 1] = history[i - 1]->saved[j];
                }
            }
            /*
             * otherwise the document is partially rebuilt from
             * the copy stored in the command structure and fully
             * reconstructed by applying all the change commands
             * following that delete
             */
            else {
                copy_from_delete(history[h - 1]);
                for (i = h + 1; i <= history_pointer + n; i++){ 
                    for (j = 0, k = history[i - 1]->ind1; j <= history[i - 1]->ind2 - history[i - 1]->ind1; j++, k++)
                        curr_document[k - 1] = history[i - 1]->saved[j];
                }
            }
            curr_lines = history[history_pointer + n - 1]->lines_reached;
    }
    history_pointer = history_pointer + n;
    sscanf(cmd_buffer, "%d,%d%c", &ind1, &ind2, &cmd);
    /*
     * executes the command that followed the
     * sequence of undo and redo commands
     */
    if(cmd == CHANGE)
        change_text(ind1, ind2);
    else if(cmd == DELETE)
        delete_text(ind1, ind2);
}

/*
 * handles input and calls the
 * corresponding functions
 */
int command_handler(){
    int ind1, ind2, q;
    char cmd;
    fgets(cmd_buffer, STR_BUF_MAX, stdin);
    cmd = cmd_buffer[strlen(cmd_buffer) - 2];
    if(cmd == QUIT)
        return 1;
    if(cmd == CHANGE || cmd == DELETE || cmd == PRINT){
        sscanf(cmd_buffer, "%d,%d%c", &ind1, &ind2, &cmd);
        if(cmd == CHANGE)
            change_text(ind1, ind2);
        if(cmd == DELETE)
            delete_text(ind1, ind2);
        if(cmd == PRINT)
            print_text(ind1, ind2);
        return 0;
    }
    if(cmd == UNDO || cmd == REDO){
        sscanf(cmd_buffer, "%d%c", &ind1, &cmd);
        if(cmd == UNDO){
            must_wipe = 1;
            q = check_sequence(ind1);
            if(q == 1)
                return 1;
            return 0;
        }
    }
    return 0;
}

int main(){
    int quit = 0;
    curr_lines = 0;
    in_history = 0;
    history_pointer = 0;
    must_wipe = 0;
    curr_document = NULL;
    history = NULL;
    last_delete = 0;
    while(quit == 0)
        quit = command_handler();
    return 0;
}
