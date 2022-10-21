/**
 * @file    functions.c
 * @author  Richard Kocián (xkocia19)
 * @brief   Knihovna funkcí pro proj2.c
 */

#include <stdbool.h>
#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

/**
 * @brief   Funkce pro prvotní inicializaci semaforů, sdílenné paměti a pro otevření výstupního souboru
 */
void init();

/**
 * @brief           Funkce pro zjištění, zda je zadaný string číslem
 *
 * @param text      Vstupní sring
 * @return true     Vstupní string je číslo
 * @return false    Vstupní string není číslo
 */
bool isInputNumber(char *text);

/**
 * @brief                   Funkce pro validaci a zapsání argumentů do proměnných
 *
 * @param argc              Počet argumentů
 * @param argv              Pole stringů argumentů
 * @return -1               Při validaci argumentů došlo k chybě
 * @return 0                Validace argumentů proběhla v pořádku
 */
int validateArguments(int argc, char *argv[]);

/**
 * @brief   Funkce pro proces kyslíku
 */
void oxygen();

/**
 * @brief       Funkce pro proces vodíku
 */
void hydrogen();

/**
 * @brief               Funkce pro zajištění čekání procesů před barrierů a jejich vypuštění při správném
 *                      počtu vodíků a kyslíků
 *
 * @param element       Znak prvku (O = kyslík, H = vodík)
 * @param ID            ID prvku
 * @param moleculeID    ID molekuly
 */
void barrierQueue(char element, int ID, int moleculeID);

/**
 * @brief               Funkce pro výpis vytváření molekuly
 *
 * @param element       Znak prvku (O = kyslík, H = vodík)
 * @param ID            ID prvku
 * @param moleculeID    ID molekuly
 */
void creatingMolecule(char element, int ID, int moleculeID);

/**
 * @brief               Funkce pro výpis, že je molekula vytvořena
 *
 * @param element       Znak prvku (O = kyslík, H = vodík)
 * @param ID            ID prvku
 * @param moleculeID    ID molekuly
 */
void createdMolecule(char element, int ID, int moleculeID);

/**
 * @brief       Funkce pro uvolnění vodíků a kyslíků z fronty, pokud již nemají dostatek atomů pro vytvoření molekuly
 */
void postRemainingProcesses(int moleculeID);

/**
 * @brief   Funkce pro unlikování semaforů, zavření otevřeného souboru a unmapování sdílené struktury
 */
void clean();

/**
 * @brief   Funkce rpo zavření všech semaforů
 */
void closeSemaphores();

/**
 * @brief Struktura sdílených porměnných
 */
typedef struct {
    int oxygenID;
    int hydrogenID;
    int moleculeID;                     // ID molekuly, počet vytvořených molekul
    int printCounter;                   // Počet vypsaných řádků
    int oxygenQueueCount;               // Počet kyslíků ve frontě
    int hydrogenQueueCount;             // Počet vodíků ve frontě
    int barrierQueueOxygens;            // Počet kyslíků ve frontě před čekáním v semaforu barrier
    int barrierQueueHydrogens;          // Počet vodíků ve frontě před čekáním v semaforu barrier
    bool isLastMoleculeCreated;         // Indikace, že již byla vytvořena poslední molekula, kterou je možno vytvořit
    int creatingMoleculesAtomsCount;    // Počet prvků, které aktuálně vytvářejí molekulu
} sharedMemory;

sharedMemory *sharedValues;
sem_t *writingSemaphore = NULL;         // semafor pro zápis do proj2.out
sem_t *mutex = NULL;                    // semafor pro zajištění správného uvolňování prvků z fronty
sem_t *hydrogenQueueSemaphore = NULL;   // fronta vodíků
sem_t *oxygenQueueSemaphore = NULL;     // fronta kyslíků
sem_t *barrier = NULL;                  // bariéra zajišťující vypuštění prvků dané molekuly najednou, post na mutex
sem_t *queueBeforeBarrier = NULL;       // fronta před bariérou
sem_t *moleculeCreated = NULL;          // signalizace vodíkům od kyslíku, že je molekula vytvořena
sem_t *atomsCreatedMolecule = NULL;     // semafor zajišťující výpis created molecule až všechny procesy vypíší, molecule creating
sem_t *atomsCreatingCount = NULL;       // semafor pro správní počítání creatingMoleculesAtomsCount
sem_t *postingBarrier = NULL;           // semafor pro zajištění vytváření další molekuly až po uvolnění všech prvků z bariéry
FILE *outputFile = NULL;

int ti = 0;
int tb = 0;
int oxygenCount = 0;
int hydrogenCount = 0;
int moleculesToCreated = 0;
int oxygensToUse = 0;
int hydrogensToUse = 0;

void init() {
    outputFile = fopen("proj2.out", "w");
    if (outputFile == NULL) {
        fprintf(stderr, "Nepodařilo se vytvořit/otevřít soubor proj2.out pro výpis logu!\n");
        clean();
        exit(1);
    }

    if ((writingSemaphore = sem_open("/xkocia19.ios.semaphore", O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) {
        fprintf(stderr, "Nepodařilo se vytvořit semafor!\n");
        clean();
        exit(1);
    }

    if ((mutex = sem_open("/xkocia19.ios.semaphore.mutex", O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) {
        fprintf(stderr, "Nepodařilo se vytvořit semafor!\n");
        clean();
        exit(1);
    }

    if ((hydrogenQueueSemaphore = sem_open("/xkocia19.ios.semaphore.hydrogen", O_CREAT | O_EXCL, 0666, 0)) ==
        SEM_FAILED) {
        fprintf(stderr, "Nepodařilo se vytvořit semafor!\n");
        clean();
        exit(1);
    }

    if ((oxygenQueueSemaphore = sem_open("/xkocia19.ios.semaphore.oxygen", O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) {
        fprintf(stderr, "Nepodařilo se vytvořit semafor!\n");
        clean();
        exit(1);
    }

    if ((barrier = sem_open("/xkocia19.ios.semaphore.barrier", O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) {
        fprintf(stderr, "Nepodařilo se vytvořit semafor!\n");
        clean();
        exit(1);
    }

    if ((queueBeforeBarrier = sem_open("/xkocia19.ios.semaphore.barrierQueue", O_CREAT | O_EXCL, 0666, 1)) ==
        SEM_FAILED) {
        fprintf(stderr, "Nepodařilo se vytvořit semafor!\n");
        clean();
        exit(1);
    }

    if ((moleculeCreated = sem_open("/xkocia19.ios.semaphore.molecule", O_CREAT | O_EXCL, 0666, 0)) ==
        SEM_FAILED) {
        fprintf(stderr, "Nepodařilo se vytvořit semafor!\n");
        clean();
        exit(1);
    }

    if ((atomsCreatedMolecule = sem_open("/xkocia19.ios.semaphore.allatoms", O_CREAT | O_EXCL, 0666, 0)) ==
        SEM_FAILED) {
        fprintf(stderr, "Nepodařilo se vytvořit semafor!\n");
        clean();
        exit(1);
    }

    if ((atomsCreatingCount = sem_open("/xkocia19.ios.semaphore.allatomscount", O_CREAT | O_EXCL, 0666, 1)) ==
        SEM_FAILED) {
        fprintf(stderr, "Nepodařilo se vytvořit semafor!\n");
        clean();
        exit(1);
    }

    if ((postingBarrier = sem_open("/xkocia19.ios.semaphore.postingbarrier", O_CREAT | O_EXCL, 0666, 0)) ==
        SEM_FAILED) {
        fprintf(stderr, "Nepodařilo se vytvořit semafor!\n");
        clean();
        exit(1);
    }

    sharedValues = mmap(NULL, sizeof(sharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sharedValues == MAP_FAILED) {
        fprintf(stderr, "Nepodařilo se vytvořit sdílenou proměnnou!\n");
        clean();
        exit(1);
    }

    sharedValues->oxygenID = 0;
    sharedValues->hydrogenID = 0;
    sharedValues->moleculeID = 0;
    sharedValues->printCounter = 0;
    sharedValues->oxygenQueueCount = 0;
    sharedValues->hydrogenQueueCount = 0;
    sharedValues->barrierQueueOxygens = 0;
    sharedValues->barrierQueueHydrogens = 0;
    sharedValues->isLastMoleculeCreated = false;
    sharedValues->creatingMoleculesAtomsCount = 0;
}

int validateArguments(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Chybný počet argumentů. Použití: ./proj2 NO NH TI TB\n");
        return -1;
    }

    oxygenCount = (int) strtol(argv[1], NULL, 0);

    if (oxygenCount < 0 || !isInputNumber(argv[1])) {
        fprintf(stderr,
                "Počet kyslíků (NO) musí být celé číslo větší nebo rovno než 0!\n Použití: ./proj2 NO NH TI TB\n");
        return -1;
    }

    hydrogenCount = (int) strtol(argv[2], NULL, 0);

    if (hydrogenCount < 0 || !isInputNumber(argv[2])) {
        fprintf(stderr,
                "Počet vodíků (NH) musí být celé číslo větší nebo rovno než 0!\n Použití: ./proj2 NO NH TI TB\n");
        return -1;
    }

    ti = (int) strtol(argv[3], NULL, 0);

    if ((ti < 0) || (ti > 1000) || (!isInputNumber(argv[3]))) {
        fprintf(stderr, "Argument TI musí být celé číslo v rozmezí než 0-1000!\n Použití: ./proj2 NO NH TI TB\n");
        return -1;
    }

    tb = (int) strtol(argv[4], NULL, 0);

    if ((tb < 0) || (tb > 1000) || (!isInputNumber(argv[4]))) {
        fprintf(stderr, "Argument TB musí být celé číslo v rozmezí než 0-1000!\n Použití: ./proj2 NO NH TI TB\n");
        return -1;
    }

    return 0;
}

bool isInputNumber(char *text) {
    for (unsigned long i = 0; i < strlen(text); i++) {
        if (!(isdigit(text[i]))) {
            return false;
        }
    }
    return true;
}

void oxygen() {
    int oxygenID = sharedValues->oxygenID;
    fprintf(outputFile, "%d: O %d: started\n", sharedValues->printCounter, oxygenID);
    fflush(outputFile);
    sem_post(writingSemaphore);

    if (ti != 0) {
        usleep(100 * (random() % ti));
    }

    sem_wait(writingSemaphore);
    sharedValues->printCounter++;
    fprintf(outputFile, "%d: O %d: going to queue\n", sharedValues->printCounter, oxygenID);
    fflush(outputFile);
    sem_post(writingSemaphore);

    sem_wait(mutex);
    sharedValues->oxygenQueueCount++;
    if (sharedValues->hydrogenQueueCount >= 2) {
        sharedValues->moleculeID++;
        sem_post(hydrogenQueueSemaphore);
        sem_post(hydrogenQueueSemaphore);
        sharedValues->hydrogenQueueCount -= 2;
        sem_post(oxygenQueueSemaphore);
        sharedValues->oxygenQueueCount--;
    } else {
        sem_post(mutex);
    }

    sem_wait(oxygenQueueSemaphore);

    barrierQueue('O', oxygenID, sharedValues->moleculeID);

    sem_wait(barrier);
    sem_wait(postingBarrier);   // čekání na dokončení uvolňování semaforu barrier

    sem_post(mutex);
}

void hydrogen() {
    int hydrogenID = sharedValues->hydrogenID;
    fprintf(outputFile, "%d: H %d: started\n", sharedValues->printCounter, hydrogenID);
    fflush(outputFile);
    sem_post(writingSemaphore);

    if (ti != 0) {
        usleep(1000 * (random() % ti));
    }

    sem_wait(writingSemaphore);
    sharedValues->printCounter++;
    fprintf(outputFile, "%d: H %d: going to queue\n", sharedValues->printCounter, hydrogenID);
    fflush(outputFile);
    sem_post(writingSemaphore);

    sem_wait(mutex);
    sharedValues->hydrogenQueueCount++;
    if (sharedValues->hydrogenQueueCount >= 2 && sharedValues->oxygenQueueCount >= 1) {
        sharedValues->moleculeID++;
        sem_post(hydrogenQueueSemaphore);
        sem_post(hydrogenQueueSemaphore);
        sharedValues->hydrogenQueueCount -= 2;
        sem_post(oxygenQueueSemaphore);
        sharedValues->oxygenQueueCount--;
    } else {
        sem_post(mutex);
    }

    sem_wait(hydrogenQueueSemaphore);

    barrierQueue('H', hydrogenID, sharedValues->moleculeID);

    sem_wait(barrier);
}

void barrierQueue(char element, int ID, int moleculeID) {
    if (sharedValues->isLastMoleculeCreated) {
        sem_wait(writingSemaphore);
        sharedValues->printCounter++;
        if (element == 'O') {
            fprintf(outputFile, "%d: O %d: not enough H\n", sharedValues->printCounter, ID);
        } else {
            fprintf(outputFile, "%d: H %d: not enough O or H\n", sharedValues->printCounter, ID);
        }
        fflush(outputFile);
        sem_post(writingSemaphore);
        sem_post(barrier);
        sem_post(postingBarrier);
        return;
    }

    creatingMolecule(element, ID, moleculeID);
    sem_wait(atomsCreatingCount);
    sharedValues->creatingMoleculesAtomsCount++;
    if (sharedValues->creatingMoleculesAtomsCount == 3) {
        sem_post(atomsCreatedMolecule);
        sharedValues->creatingMoleculesAtomsCount -= 3;
    }
    sem_post(atomsCreatingCount);


    if (element == 'O') {
        sem_wait(atomsCreatedMolecule);
        if (tb != 0) {
            usleep(1000 * (random() % tb));
        }
        sem_post(moleculeCreated);
        sem_post(moleculeCreated);
        sem_post(moleculeCreated);
    }

    sem_wait(moleculeCreated);

    createdMolecule(element, ID, moleculeID);

    sem_wait(queueBeforeBarrier);
    if (element == 'O') {
        sharedValues->barrierQueueOxygens++;
    } else {
        sharedValues->barrierQueueHydrogens++;
    }

    if (sharedValues->barrierQueueOxygens >= 1 && sharedValues->barrierQueueHydrogens >= 2) {
        sharedValues->barrierQueueOxygens--;
        sharedValues->barrierQueueHydrogens -= 2;
        postRemainingProcesses(moleculeID);     // uvolnění front prvků, kterým již nezbývají další prvky pro vytvoření další molekuly
        sem_post(barrier);
        sem_post(barrier);
        sem_post(barrier);
        sem_post(postingBarrier);   // indikace, že bylo dokončeno uvolňování semaforu barrier
    }
    sem_post(queueBeforeBarrier);
}

void creatingMolecule(char element, int ID, int moleculeID) {
    sem_wait(writingSemaphore);
    sharedValues->printCounter++;
    fprintf(outputFile, "%d: %c %d: creating molecule %d\n", sharedValues->printCounter, element, ID, moleculeID);
    fflush(outputFile);
    sem_post(writingSemaphore);
}

void createdMolecule(char element, int ID, int moleculeID) {
    sem_wait(writingSemaphore);
    sharedValues->printCounter++;
    fprintf(outputFile, "%d: %c %d: molecule %d created\n", sharedValues->printCounter, element, ID, moleculeID);
    fflush(outputFile);
    sem_post(writingSemaphore);
}

void postRemainingProcesses(int moleculeID) {
    if (moleculeID == moleculesToCreated && !sharedValues->isLastMoleculeCreated) {
        sharedValues->isLastMoleculeCreated = true;
        for (int i = 0; i < oxygenCount - oxygensToUse; i++) {
            sem_post(oxygenQueueSemaphore);
        }
        for (int i = 0; i < hydrogenCount - hydrogensToUse; i++) {
            sem_post(hydrogenQueueSemaphore);
        }
    }
}

void clean() {
    if (outputFile != NULL) {
        fclose(outputFile);
    }
    sem_unlink("/xkocia19.ios.semaphore");

    sem_unlink("/xkocia19.ios.semaphore.mutex");

    sem_unlink("/xkocia19.ios.semaphore.hydrogen");

    sem_unlink("/xkocia19.ios.semaphore.oxygen");

    sem_unlink("/xkocia19.ios.semaphore.barrier");

    sem_unlink("/xkocia19.ios.semaphore.barrierQueue");

    sem_unlink("/xkocia19.ios.semaphore.molecule");

    sem_unlink("/xkocia19.ios.semaphore.allatoms");

    sem_unlink("/xkocia19.ios.semaphore.allatomscount");

    sem_unlink("/xkocia19.ios.semaphore.postingbarrier");

    closeSemaphores();

    munmap(sharedValues, sizeof(sharedMemory));
}

void closeSemaphores() {
    sem_close(writingSemaphore);

    sem_close(mutex);

    sem_close(hydrogenQueueSemaphore);

    sem_close(oxygenQueueSemaphore);

    sem_close(barrier);

    sem_close(queueBeforeBarrier);

    sem_close(moleculeCreated);

    sem_close(atomsCreatedMolecule);

    sem_close(atomsCreatingCount);

    sem_close(postingBarrier);
}
// FUNCTIONS_C
