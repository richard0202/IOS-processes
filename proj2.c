/**
 * @file    functions.c
 * @author  Richard Kocián (xkocia19)
 * @brief   Implementace projektu 2 do předmětu IOS
 */

#include "functions.c"
#include <signal.h>


int main(int argc, char *argv[]) {
    init();

    if (validateArguments(argc, argv) == -1) {
        clean();
        exit(1);
    }

    moleculesToCreated = hydrogenCount / 2;     // počet molekul, které budou vytvořeny
    oxygensToUse = 0;       // počet kyslíků, které budou použity pro vytváření molekul
    hydrogensToUse = 0;     // počet vodíků, které budou použity pro vytváření molekul
    if (oxygenCount < moleculesToCreated) {
        moleculesToCreated = oxygenCount;
        oxygensToUse = oxygenCount;
    } else {
        oxygensToUse = moleculesToCreated;
    }
    hydrogensToUse = moleculesToCreated * 3 - oxygensToUse;

    // Vytvoření pole pid_t všech procesů potomků, co budou vytvořeny
    pid_t processes[oxygenCount + hydrogenCount];

    // Uvolnění front, pokud nebude vytvořena žádná molekula
    postRemainingProcesses(0);

    // Cyklus, který vytvoří daný počet procesů vodíků a kyslíků
    for (int i = 0; i < oxygenCount + hydrogenCount; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            sem_wait(writingSemaphore);
            sharedValues->printCounter++;
            if (i < oxygenCount) {
                sharedValues->oxygenID++;
                oxygen();
            } else if (i >= oxygenCount) {
                sharedValues->hydrogenID++;
                hydrogen();
            }
            closeSemaphores();
            fclose(outputFile);
            exit(0);
        } else if (pid == -1) {
            fprintf(stderr, "Došlo k chybě při vytváření potomka!\n");
            for (int j = 0; j < i; j++) {
                kill(processes[j],SIGKILL);
            }
            clean();
            exit(1);
        } else {
            processes[i] = pid;
            fflush(outputFile);
        }
    }

    // čekání na dokončení procesů potomků
    for (int i = 0; i < oxygenCount + hydrogenCount; i++) {
        wait(&processes[i]);
    }

    clean();

    exit(0);
}
// PROJ2_C
