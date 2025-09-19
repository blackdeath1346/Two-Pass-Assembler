#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 256
#define MAX_MACRO_NAME 50
#define MAX_PARAMS 10
#define MAX_BODY_LINES 50
#define MAX_MACROS 50
#define STACK_SIZE 1000

typedef struct {
    char label[MAX_MACRO_NAME];
    char params[MAX_PARAMS][MAX_MACRO_NAME];
    int paramCount;
    char body[MAX_BODY_LINES][MAX_LINE];
    int bodyLineCount;
} Macro;

typedef struct {
    Macro *macro;
    char args[MAX_PARAMS][MAX_MACRO_NAME];
    int currentLine;
} MacroContext;

// Global variables
Macro macros[MAX_MACROS];
int macroCount = 0;
MacroContext stack[STACK_SIZE];
int stackTop = -1;

// Stack operations
void push(MacroContext context) {
    if (stackTop >=STACK_SIZE - 1) {
        printf("Error: Macro stack overflow.\n");
        exit(1);
    }
    stack[++stackTop] = context;
}

MacroContext pop() {
    if (stackTop < 0) {
        printf("Error: Macro stack underflow.\n");
        exit(1);
    }
    return stack[stackTop--];
}

int isStackEmpty() {
    return stackTop < 0;
}

// Step 1: Process Macro Definitions
void process_macro_definitions() {
    FILE *inputFile = fopen("input.txt", "r");
    FILE *macroFile = fopen("macrodef.txt", "w");
    FILE *tempFile = fopen("temp.txt", "w"); // Temporary file to hold non-macro lines

    if (!inputFile || !macroFile || !tempFile) {
        printf("Error: Unable to open one or more files.\n");
        exit(1);
    }

    char line[MAX_LINE];
    int inMacro = 0;
    Macro *currentMacro = NULL;

    while (fgets(line, MAX_LINE, inputFile)) {
        char label[MAX_MACRO_NAME], opcode[MAX_MACRO_NAME], operand[MAX_LINE];
        sscanf(line, "%s %s %[^\n]", label, opcode, operand);

        if (strcmp(opcode, "MACRO") == 0) {
            // Start of a macro definition
            inMacro = 1;
            currentMacro = &macros[macroCount++];
            strcpy(currentMacro->label, label);
            currentMacro->paramCount = 0;
            currentMacro->bodyLineCount = 0;

            // Extract parameters
            char *token = strtok(operand, ", ");
            while (token) {
                strcpy(currentMacro->params[currentMacro->paramCount++], token);
                token = strtok(NULL, ", ");
            }

            fprintf(macroFile, "%s", line); // Save macro header to macro file
        } else if (strcmp(opcode, "MEND") == 0) {
            // End of a macro definition
            inMacro = 0;
            fprintf(macroFile, "%s", line); // Save MEND to macro file
        } else if (inMacro) {
            // Inside the macro body
            strcpy(currentMacro->body[currentMacro->bodyLineCount++], line);
            fprintf(macroFile, "%s", line); // Save macro body to macro file
        } else {
            // Outside macro definition, write to temp file
            fprintf(tempFile, "%s", line);
        }
    }

    fclose(inputFile);
    fclose(macroFile);
    fclose(tempFile);
}

// Step 2: Expand Macros with Nested Support
void expand_macros() {
    FILE *inputFile = fopen("temp.txt", "r");
    FILE *expandedFile = fopen("expanded.txt", "w");

    if (!inputFile || !expandedFile) {
        printf("Error: Unable to open one or more files.\n");
        exit(1);
    }

    char line[MAX_LINE];

    while (fgets(line, MAX_LINE, inputFile)) {
        char label[MAX_MACRO_NAME], opcode[MAX_MACRO_NAME], operand[MAX_LINE];
        sscanf(line, "%s %s %[^\n]", label, opcode, operand);

        int macroFound = 0;

        // Check if the line is a macro call
        for (int i = 0; i < macroCount; i++) {
            if (strcmp(label, macros[i].label) == 0) {
                // Macro found, initialize context and push onto the stack
                macroFound = 1;
                sscanf(line, "%s %[^\n]", label, operand);

                // Parse arguments
                char args[MAX_PARAMS][MAX_MACRO_NAME];
                int argCount = 0;
                char *token = strtok(operand, ", ");
                while (token) {
                    strcpy(args[argCount++], token);
                    token = strtok(NULL, ", ");
                }

                MacroContext context = {&macros[i], {0}, 0};
                for (int j = 0; j < argCount; j++) {
                    strcpy(context.args[j], args[j]);
                }
                push(context);

                // DFS-style stack processing
                while (!isStackEmpty()) {
                    MacroContext current = pop();

                    for (int j = current.currentLine; j < current.macro->bodyLineCount; j++) {
                        char expandedLine[MAX_LINE];
                        strcpy(expandedLine, current.macro->body[j]);

                        // Replace parameters with arguments
                        for (int k = 0; k < current.macro->paramCount; k++) {
                            char *pos = strstr(expandedLine, current.macro->params[k]);
                            if (pos) {
                                char temp[MAX_LINE];
                                strncpy(temp, expandedLine, pos - expandedLine);
                                temp[pos - expandedLine] = '\0';
                                strcat(temp, current.args[k]);
                                strcat(temp, pos + strlen(current.macro->params[k]));
                                strcpy(expandedLine, temp);
                            }
                        }

                        // Check for nested macro calls
                        char nestedLabel[MAX_MACRO_NAME], nestedOpcode[MAX_MACRO_NAME], nestedOperand[MAX_LINE];
                        sscanf(expandedLine, "%s %s %[^\n]", nestedLabel, nestedOpcode, nestedOperand);

                        int nestedFound = 0;
                        for (int m = 0; m < macroCount; m++) {
                            if (strcmp(nestedLabel, macros[m].label) == 0) {
                                nestedFound = 1;
                                sscanf(expandedLine, "%s %[^\n]", nestedLabel, nestedOperand);
                                // Parse nested arguments
                                char nestedArgs[MAX_PARAMS][MAX_MACRO_NAME];
                                int nestedArgCount = 0;
                                token = strtok(nestedOperand, ", ");
                                while (token) {
                                    strcpy(nestedArgs[nestedArgCount++], token);
                                    token = strtok(NULL, ", ");
                                }

                                // Push the nested macro context onto the stack
                                MacroContext nestedContext = {&macros[m], {0}, 0};
                                for (int n = 0; n < nestedArgCount; n++) {
                                    strcpy(nestedContext.args[n], nestedArgs[n]);
                                }

                                // Update the current line to continue later and push it back to the stack
                                current.currentLine = j + 1;
                                push(current);

                                // Push the nested macro context for immediate processing
                                push(nestedContext);

                                // Break to process the nested macro first
                                break;
                            }
                        }

                        if (!nestedFound) {
                            // Write the expanded line if no nested macro is found
                            fprintf(expandedFile, "%s", expandedLine);
                        }

                        // Stop processing the current macro body if a nested macro is found
                        if (nestedFound) {
                            break;
                        }
                    }
                }
            }
        }

        if (!macroFound) {
            // Write non-macro lines to expanded.txt
            fprintf(expandedFile, "%s", line);
        }
    }

    fclose(inputFile);
    fclose(expandedFile);
}

void display();
void passOne();
void passTwo();

int main() {
    printf("Processing Macro Definitions...\n");
    process_macro_definitions();
    printf("Macro Definitions Processed.\n");

    printf("Expanding Macros...\n");
    expand_macros();
    printf("Macro Expansion Completed.\n");

    printf("Executing Pass 1...\n");
    passOne();
    printf("Pass 1 Completed.\n\n");

    printf("Executing Pass 2...\n");
    passTwo();
    printf("Pass 2 Completed.\n\n");

    display();

    return 0;
}

void passOne() {
    int locctr, start, length;
    char label[10], opcode[10], operand[10], code[10], mnemonic[3];
    FILE *fp1, *fp2, *fp3, *fp4, *fp5;

    // Open files
    fp1 = fopen("expanded.txt", "r");
    fp2 = fopen("optab.txt", "r");
    fp3 = fopen("symtab1.txt", "w");
    fp4 = fopen("symtab.txt", "w");
    fp5 = fopen("length.txt", "w");

    if (!fp1 || !fp2 || !fp3 || !fp4 || !fp5) {
        printf("Error: Unable to open one or more files.\n");
        exit(1);
    }

    fscanf(fp1, "%s\t%s\t%s", label, opcode, operand);

    if (strcmp(opcode, "START") == 0) {
        start = atoi(operand);
        locctr = start;
        fprintf(fp4, "\t%s\t%s\t%s\n", label, opcode, operand);
        fscanf(fp1, "%s\t%s\t%s", label, opcode, operand);
    } else {
        locctr = 0;
    }

    while (strcmp(opcode, "END") != 0) {
        fprintf(fp4, "%d\t%s\t%s\t%s\n", locctr, label, opcode, operand);

        if (strcmp(label, "-") != 0 && strlen(label) > 0) { // Valid label (non-empty)
            // Check for the operand type and handle size
            int size = 0;
            if (strcmp(opcode, "WORD") == 0) {
                size = 3;
            } else if (strcmp(opcode, "RESW") == 0) {
                size = 3 * atoi(operand);
            } else if (strcmp(opcode, "BYTE") == 0) {
                size = (strlen(operand) - 3); // Length of constant
            } else if (strcmp(opcode, "RESB") == 0) {
                size = atoi(operand);
            }

            // Add symbol and its size/type to the symbol table
            fprintf(fp3, "%s\t%d\t%s\t%s\t%d\n", operand, locctr, label, opcode, size); // Add size to symtab
        }

        // Reset file pointer for optab search
        rewind(fp2);
        int found = 0;
        while (fscanf(fp2, "%s\t%s", code, mnemonic) != EOF) {
            if (strcmp(opcode, code) == 0) {
                locctr += 3;
                found = 1;
                break;
            }
        }

        if (!found) { // Handle directives
            if (strcmp(opcode, "WORD") == 0) {
                locctr += 3;
            } else if (strcmp(opcode, "RESW") == 0) {
                locctr += 3 * atoi(operand);
            } else if (strcmp(opcode, "BYTE") == 0) {
                locctr += (strlen(operand) - 3); // Length of constant
            } else if (strcmp(opcode, "RESB") == 0) {
                locctr += atoi(operand);
            } else {
                printf("Error: Invalid opcode %s\n", opcode);
                exit(1);
            }
        }

        fscanf(fp1, "%s\t%s\t%s", label, opcode, operand);
    }

    fprintf(fp4, "%d\t%s\t%s\t%s\n", locctr, label, opcode, operand);
    fclose(fp1);
    fclose(fp2);
    fclose(fp3);
    fclose(fp4);

    length = locctr - start;
    fprintf(fp5, "%d", length);
    fclose(fp5);

    printf("The length of the code: %d\n", length);
}


void passTwo() {
    char label[10], opcode[10], operand[10], symbol[10];
    char mnemonic[50][15], code[50][15];
    int instruction_count = 0;
    int address, start, add, i;
    char obj_code[10];
    FILE *fp1, *fp2, *fp3, *fp4;

    // Load optab into memory
    FILE *fp_optab = fopen("optab.txt", "r");
    if (!fp_optab) {
        printf("Error: Unable to open optab.txt.\n");
        exit(1);
    }
    while (fscanf(fp_optab, "%s\t%s", mnemonic[instruction_count], code[instruction_count]) != EOF) {
        instruction_count++;
    }
    fclose(fp_optab);

    // Open files
    fp1 = fopen("output.txt", "w");
    fp2 = fopen("symtab1.txt", "r");
    fp3 = fopen("symtab.txt", "r");
    fp4 = fopen("objcode.txt", "w");

    if (!fp1 || !fp2 || !fp3 || !fp4) {
        printf("Error: Unable to open one or more files.\n");
        exit(1);
    }

    fscanf(fp3, "%s\t%s\t%s", label, opcode, operand);
    if (strcmp(opcode, "START") == 0) {
        fprintf(fp1, "\t%s\t%s\t%s\n", label, opcode, operand);
        fprintf(fp4, "H^%s^00%s\n", label, operand);
        fscanf(fp3, "%d%s%s%s", &address, label, opcode, operand);
        start = address;
    }

    while (strcmp(opcode, "END") != 0) {
        int found = 0;
        for (i = 0; i < instruction_count; i++) {
            if (strcmp(opcode, mnemonic[i]) == 0) {
                found = 1;
                break;
            }
        }

        if (found) {
            rewind(fp2);
            int symbol_found = 0;
            while (fscanf(fp2, "%s\t%d", symbol, &add) != EOF) {
                if (strcmp(operand, symbol) == 0) {
                    symbol_found = 1;
                    break;
                }
            }
            if (!symbol_found) {
                printf("Error: Symbol %s not found in SYMTAB.\n", operand);
                exit(1);
            }
            sprintf(obj_code, "%s%04d", code[i], add);
            fprintf(fp1, "%d\t%s\t%s\t%s\t%s\n", address, label, opcode, operand, obj_code);
            fprintf(fp4, "^%s", obj_code);
        } else if (strcmp(opcode, "BYTE") == 0) {
            fprintf(fp1, "%d\t%s\t%s\t%s\t", address, label, opcode, operand);
            for (i = 2; i < strlen(operand) - 1; i++) {
                fprintf(fp1, "%X", operand[i]);
                fprintf(fp4, "%X", operand[i]);
            }
            fprintf(fp1, "\n");
        } else if (strcmp(opcode, "WORD") == 0) {
            sprintf(obj_code, "%06X", atoi(operand));
            fprintf(fp1, "%d\t%s\t%s\t%s\t%s\n", address, label, opcode, operand, obj_code);
            fprintf(fp4, "^%s", obj_code);
        } else {
            fprintf(fp1, "%d\t%s\t%s\t%s\n", address, label, opcode, operand);
        }

        fscanf(fp3, "%d%s%s%s", &address, label, opcode, operand);
    }

    fprintf(fp1, "%d\t%s\t%s\t%s\n", address, label, opcode, operand);
    fprintf(fp4, "\nE^00%d\n", start);

    fclose(fp1);
    fclose(fp2);
    fclose(fp3);
    fclose(fp4);
}

void display() {
    char ch;
    FILE *fp1, *fp2, *fp3, *fp4;

    printf("\nThe contents of Intermediate file:\n\n");
    fp3 = fopen("intermediate.txt", "r");
    while ((ch = fgetc(fp3)) != EOF) {
        putchar(ch);
    }
    fclose(fp3);

    printf("\n\nThe contents of Symbol Table:\n\n");
    fp2 = fopen("symtab.txt", "r");
    while ((ch = fgetc(fp2)) != EOF) {
        putchar(ch);
    }
    fclose(fp2);

    printf("\n\nThe contents of Output file:\n\n");
    fp1 = fopen("output.txt", "r");
    while ((ch = fgetc(fp1)) != EOF) {
        putchar(ch);
    }
    fclose(fp1);

    printf("\n\nThe contents of Object Code file:\n\n");
    fp4 = fopen("objcode.txt", "r");
    while ((ch = fgetc(fp4)) != EOF) {
        putchar(ch);
    }
    fclose(fp4);
}
