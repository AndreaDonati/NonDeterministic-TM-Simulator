#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <stdbool.h>

/**
QUICK OVERVIEW:

Durante la fase progettuale mi sono reso conto che l'esecuzione di transizioni e il doversi salvare informazioni
molto diverse in contemporanea per supportare il non determinismo andavano a creare un albero di configurazioni
che la macchina assumeva durante l'evoluzione.
A partire dalla configurazione iniziale, la radice, se ne creano tante altre quante sono le transizioni non deterministiche
che è possibile percorrere leggendo il primo carattere della stringa in input, e questo procedimento viene reiterato su ogniuna
delle configurazioni appena create. La soluzione greedy è quella di implementare una funzione ricorsiva, ma appena provato mi sono
reso conto di sforare la memoria solamente con lo stack delle chiamate a funzione, quindi ho dovuto implementare una soluzione iterativa.
Questa soluzione si basa sullo correre l'albero in ampiezza (breadth-first), analizzando un intero livello per computare il livello successivo,
e scorrere così tutto l'albero fino ad un massimo indice di profondità indicato nella specifica della macchina in input.

Le particolarità di questa soluzione stanno nell'organizzazione delle transizioni e nella gestione dei nastri.
-> TRANSIZIONI:
    Le transizioni, come anche scritto in seguito, sono organizzate in una matrice di liste, indicizzate secondo
    lo stato di partenza e il carattere letto.
-> NASTRI:
    I nastri in ogni configurazione sono salvati "a metà", cioè come parte destra e parte sinistra.
    Questo perchè ho avuto problemi di tempo legati all'estensione del nastro a sinstra, quindi
    mi è sembrato c"onveniente trattare il nastro come se estendessi "sempre a destra".
*/


//useful costanti
#define STATE_INPUT_LENGTH 123
#define STRICT_TRANSITIONS_ARRAY_LENGHT 64
#define ACC_INDEX 63
#define ACC_DIRECTION_FLAG 'A'
#define DEFAULT_INPUT_BLOCK_LENGTH 256
#define BLANK 95
//risultati
#define SI 1
#define NO 0
#define U 2

//struttura che rappresenta una transizione per come viene acquisita dall'input
typedef struct {
    int state;          //stato corrente
    char read;          //carattere letto
    char write;         //carattere da scrivere
    char direction;     //direzione in cui spostare la testina di lettura/scrittura
    int next_state;     //stato prossimo
} transition_type;

//struttura che rappresenta una transizione per come viene trattata durante
//l'elaborazione
typedef struct strict_transition {
    char write;
    char direction;
    int next_state;
    struct strict_transition *next;
}strict_transition;

//struttura che rappresenta la configurazione di una MT in un determinato istante
typedef struct MTConfig{
    int state_num;      //numero dello stato in cui si trova la macchina
    char* tape;         //puntatore al nastro "attivo"
    char* rightTape;    //puntatore alla porzione di destra del nastro
    char* leftTape;     //puntatore alla porzione di sinistra del nastro
    int leftTapeLength; //lunghezza del nastro di sinistra -> salvata per non dover usare strlen ogni volta
    int rightTapeLength;//lunghezza del nastro di destra   -> anche perchè incremento sempre di un valore di default
    int index;          //valore intero che indica la posizione della testina all'interno del nastro attivo
    struct MTConfig *next;  //puntatore alla prossima configurazione, quella che deve seguire nell'elaborazione
    struct MTConfig *nextInAllocationList;  //puntatore alla prossima configurazione salvata nella lista di tutte le config. allocate
}MTConfig;
//NOTA:
//ho scelto di rappresentare il nastro in due porzioni perchè mi sono accorto che in test come
//ToCOrNotToC andavo sempre fuori tempo. Ho capito che questo era dovuto alla maniera con cui
//precedentemente estendevo il nastro a sinistra, cioè reallocando, mettendo un certo numero
//di blank all'inzio e poi ricopiando tutto il nastro. Visto che ToC (almeno il pubblico)
//estende molto il nastro sia a destra sia a sinistra, la mia strategia di estensione a sinistra
//era fallimentare.
//In questo modo invece la reallocazione avviene sempre solo come estensione verso destra,
//impiegando meno tempo, con il solo accorgimento di switchare correttamente i nastri.

//struttura semplice per listare le transizioni acquisite da input
typedef struct {
    transition_type* transition;
    struct list_type* linked_state; //prossimo elemento nella lista
}list_type;

//---------------------------------------VETTORE PER L'HASH DEI CARATTERI LETTI NEGLI STATI--------------------------------------------------
int hash_array[STATE_INPUT_LENGTH];
//questo vettore è inizialzzato all'avvio del programma e ha la finalità di diminuire al minimo la quantità
//di informazione che ogni transizione contiene. Infatti rende possibile avere, per ogni transizione, un vettore
//di indicizzazione da 64 elementi e non da 128, come sarebbe necessario per indicizzare tutti i caratteri della
//ASCII table.
//Il suo funzionamento è piuttosto semplice, alla posizione corrispondente ad un dato carattere vi è l'indice
//nel quale sono contenute le transizioni che hanno tale carattere come carattere letto all'interno del vettore
//stateTransitionIndexer.
//-------------------------------------------------------------------------------------------------------------------------------------------

//---------------------------------------LISTA GOBALE DELLE TRANSIZIONI----------------------------------------------------------------------
list_type* transitions_list_head = NULL;
//una semplice lista che viene usata per conservare temporaneamente le transizioni date in input
//-------------------------------------------------------------------------------------------------------------------------------------------

//---------------------------------------NUMERO MASSIMO DI STATI PRESENTI NELLA MT-----------------------------------------------------------
int max_state = 0;
//-------------------------------------------------------------------------------------------------------------------------------------------

//---------------------------------------VETTORE INDICIZZATORE DI STATI E TRANSIZIONI A RUNTIME----------------------------------------------
strict_transition ***stateTransitionIndexer = NULL;
//questo vettore può essere interpretato come una matrice di puntatori (e la possibiltà di tripla deferenziazione lo fa già capire)
//Sulle righe abbiamo gli stati della MT
//Sulle colonne abbiamo l'indicizzazione di tutti i caratteri considerati nell'evoluzione della MT (numeri, lettere minuscole e maiuscole)
//ogni cella contiene il puntatore di accesso alla lista di transizioni che partono da un certo stato (riga), leggendo un certo
//carattere (colonna)
//-------------------------------------------------------------------------------------------------------------------------------------------

//---------------------------------------NUMERO MASSIMO DI PASSI DA FARE DURANTE L'ESECUZIONE PRIMA DI DARE U COME OUTPUT--------------------
long max_steps = 0;
//-------------------------------------------------------------------------------------------------------------------------------------------

//---------------------------------------FLAG CHE SEGNALA LA FINE DEL FILE DI INPUT----------------------------------------------------------
bool file_finished = false;
//-------------------------------------------------------------------------------------------------------------------------------------------

//---------------------------------------FLAG CHE SEGNALA SE HO SVOLTO QUALCOSA NELLE TRANSIZIONI O NO---------------------------------------
//mi serve per capire se ho eseguito una transizione durante l'evoluzione della macchina
bool didSomething = false;
//-------------------------------------------------------------------------------------------------------------------------------------------

//---------------------------------------LISTE DI CONFIGURAZIONI DA ESEGUIRE / CREATE--------------------------------------------------------
MTConfig *primaryConfigList = NULL;
MTConfig *secondaryConfigList = NULL;
MTConfig *toBeFreedList = NULL;
MTConfig **listaMTAllocate = NULL;

//queste liste vengono usate durante l'evoluzione della macchina
//la lista primaria è la lista delle configurazioni che sono in esecuzione in questo momento
//nella lista secondaria vengono inserite tutte le configurazioni che si creano durante l'esecuzione di quelle
//contenute nella lsita primaria
//nella lista da essere liberata sono presenti tutte le configurazioni di cui può essere fatta la free
//nella lista delle MT allocate ci sono tutte le congifurazioni allocate durante l'esecuzione, che quando vengono
//liberate vengono rimosse. E' stata inserita perchè mi perdevo qualche configurazione durante le diverse
//evoluzioni andando out of memory spesso e volentieri.
//-------------------------------------------------------------------------------------------------------------------------------------------

int destinatiAEssereU = 0;

//aquisisce le transizioni da stdin
void getTransitions();
//organizza le transizioni acuisite nello stateTransitionIndexer
void groupTransitions();
//acquisisce lo stato di accettazione
void getAcceptation();
//acquisisce il numero massimo di passi
void getMaxSteps();
//acquisisce una stringa di input da stdin
char *acquireInputString();
//processa la stringa passata simulando la macchina salvata
int processInputString(char *inputHead);
//esegue la transizione passata come parametro sulla configurazione passata come parametro
bool executeTransition(MTConfig *pConfig, strict_transition *currentTransition);
//restituisce true se la configurazione si trova in uno stato di accettazione
//false altrimenti
bool isAcceptation(MTConfig *pConfig);
//restituisce una deep-copy della configurazione passata
//quindi copiando anche i nastri
MTConfig *copyMTConfig(MTConfig *toCopy);
//restituisce una copia del nastro passato
char *copyTape(const char* tapeToCopy,int length);
//muove la testina di una configurazione in un certo senso
void moveHead(char direction, MTConfig **currConfig);
//inserisce nella secondaryConfigList la configurazione passata
void produceMT(MTConfig **pState);
//restituisce l'elemento in testa alla primaryConfigList e lo toglie dalla lista
MTConfig *consumeMT();
//libera la lista passata
void freeOldList(MTConfig **pState);
//rimuove uno stato dalla lista delle MT allocate
void removeFromListaAllocazione(MTConfig *pConfig);

int main() {

    //inizializzo il vettore di hash
    for (int i = 0; i < STATE_INPUT_LENGTH; ++i)
        hash_array[i] = -1;
    //definisco il vettore di hash
    int hash_array_index = -1; // 0 o -1?
    for (int j = (int) 'A'; j <= 'Z'; ++j)
        hash_array[j] = ++hash_array_index;
    for (int j = (int) 'a'; j <= 'z'; ++j)
        hash_array[j] = ++hash_array_index;
    for (int j = (int) '0'; j <= '9'; ++j)
        hash_array[j] = ++hash_array_index;
    hash_array[BLANK] = hash_array_index+1;

    //for (int k = 0; k < STATE_INPUT_LENGTH; ++k)
    //    printf("(%c) [%d] -> %d\n",k,k,hash_array[k]);

    //variabile intera che conterrà il risultato della computazione
    int ris = 0;

    bool first = true;

    //vettore di caratteri pieno di blank, per vedere se ho acquisito una stringa vuota
    char* test = calloc(DEFAULT_INPUT_BLOCK_LENGTH, sizeof(char));
    memset(test,BLANK,DEFAULT_INPUT_BLOCK_LENGTH);
    //---------------
    getAcceptation();
    //---------------
    getMaxSteps();
    //---------------
    //acquisisco la stringa run
    char* run = malloc(sizeof(char)*4);
    scanf("%s",run);
    free(run);
    //puntatore alla stringa acquisita
    char** inputString = malloc(sizeof(char*));
    *inputString = NULL;

    //simulo la macchina con tutte le stringhe di input presenti nel file
    //quindi termino
    while(!file_finished){
        //leggo la stringa di input
        *inputString = acquireInputString();
        //se capisco che ho finito il file, termino
        if((*inputString) == NULL) {
            break;
        }
        if(strcmp((*inputString),test) == 0) {
            break;
        }
        //processo la stringa
        ris = processInputString((*inputString));
        if(!first){
            printf("\n");
        }
        first = false;
        if(ris == U) {
            printf("U");
        }
        else {
            printf("%d",ris);
        }
        fflush(stdout);
    }

    return 0;
}

int processInputString(char *inputHead) {
    //creo la configurazione di partenza, quella da cui si parte sempre, che ha come stato lo stato 0;
    MTConfig *currConfig = malloc(sizeof(MTConfig));
    currConfig->next = NULL;
    currConfig->state_num = 0;

    currConfig->rightTape = inputHead;
    currConfig->rightTapeLength = (int) strlen(inputHead);

    currConfig->leftTape = calloc(DEFAULT_INPUT_BLOCK_LENGTH,sizeof(char));
    memset(currConfig->leftTape,BLANK,DEFAULT_INPUT_BLOCK_LENGTH);
    currConfig->leftTapeLength = DEFAULT_INPUT_BLOCK_LENGTH;

    currConfig->tape = currConfig->rightTape;
    currConfig->index = 0;

    currConfig->nextInAllocationList = *listaMTAllocate;
    *listaMTAllocate = currConfig;

    primaryConfigList = NULL;
    secondaryConfigList = NULL;

    long nSteps = 0;
    int numeroConfigurazioniNonAccettazione = 0;
    strict_transition *currentTransition = NULL;

    //il contatore viene aumentato ogni volta che eseguo un passo
    //cioè eseguo tutte le transizioni percorribili da una certa configurazione
    //poi passo all'altra nella lista ecc ecc, scorrendo l'albero in ampiezza
    //in pratica la lista primaria indica il livello n, mentre la secondaria
    //il livello n+1 di un albero di computazioni
    //quando arrivo a una certa profondità smetto e stampo il risultato
    //se non sono prima entrato in uno stato di accettazione
    for (nSteps = 0;  nSteps < max_steps+1; ++nSteps) {
        //scorro tutta la lista delle configurazioni ed eseguo tutte le transizioni che posso
        while (currConfig != NULL) {
            //se la configurazione corrente si trova in uno stato di accettazione, esco dicendo 1
            //e liberando la memoria residua.
            if (isAcceptation(currConfig)) {
                freeOldList(&toBeFreedList);
                return SI;
            }
            //prendo dallo stateTransitionIndexer la lista di transizioni da eseguire (se presente)
            if (stateTransitionIndexer[currConfig->state_num] != NULL && hash_array[(int)currConfig->tape[currConfig->index]] != -1)
                currentTransition = stateTransitionIndexer[currConfig->state_num][hash_array[(int) currConfig->tape[currConfig->index]]];
            else {
                    //se non è presente nessuna transizione da eseguire vuol dire che sono in uno stato pozzo, aumento
                    //il numero di configurazioni di non accettazione e libero la configurazione corrente
                currConfig->next = NULL;
                removeFromListaAllocazione(currConfig);
                free((*currConfig).rightTape);
                free((*currConfig).leftTape);
                free(currConfig);
                currConfig = NULL;
                numeroConfigurazioniNonAccettazione++;
                currentTransition = NULL;
            }
            //eseguo tutte le transizioni che partono da un certo stato
            didSomething = false;
            while (currentTransition != NULL) {
                executeTransition(currConfig, currentTransition);
                currentTransition = currentTransition->next;
            }
            //se le transizioni che eseguo non fanno niente allora libero la configurazione
            if (!didSomething) {
                if(currConfig != NULL){
                    currConfig->next = NULL;
                    removeFromListaAllocazione(currConfig);
                    free((*currConfig).leftTape);
                    free((*currConfig).rightTape);
                    free(currConfig);
                }
                numeroConfigurazioniNonAccettazione++;
            }
            //prendo la configurazione successiva dalla lista
            currConfig = consumeMT();
        }
        //switcho le liste, in modo tale per cui riprendendo l'esecuzione del
        //for la lista delle configuraioni da eseguire sia quella
        //delle configurazioni appena generate
        primaryConfigList = secondaryConfigList;
        currConfig = consumeMT();
        toBeFreedList = secondaryConfigList;
        //se currConfig è NULL vuol dire che non ho più configurazioni da eseguire, posso quindi ritornare 0, perchè sicuramente
        //la stringa sarà non accettata
        if(currConfig == NULL) break;
        secondaryConfigList = NULL;
    }

    //libero l'unica lista che devo liberare in teoria
    freeOldList(&toBeFreedList);
    toBeFreedList = NULL;

    //libero la lista delle MT allocate
    MTConfig *temp = NULL;
    while ((*listaMTAllocate) != NULL) {
        temp = (*listaMTAllocate)->nextInAllocationList;
            free((*(*listaMTAllocate)).leftTape);
            free((*(*listaMTAllocate)).rightTape);
            free((*listaMTAllocate));
        (*listaMTAllocate) = temp;
    }
    (*listaMTAllocate) = NULL;
    if(destinatiAEssereU > 0)
        return U;
    if(nSteps > max_steps) {
        return U;
    }
    if(numeroConfigurazioniNonAccettazione > 0) {
        return NO;
    }
    return U;
}
//normale rimozione da una lista
void removeFromListaAllocazione(MTConfig *pConfig) {
    if(*listaMTAllocate == NULL)
        return;
    MTConfig * tmp = *listaMTAllocate;
    MTConfig * listToDel;
    if(tmp == pConfig) {
        listToDel = *listaMTAllocate;
        *listaMTAllocate = (*listaMTAllocate)->nextInAllocationList;
        return;
    }
    while(tmp->nextInAllocationList != NULL) {
        if(tmp->nextInAllocationList == pConfig) {
            tmp->nextInAllocationList = tmp->nextInAllocationList->nextInAllocationList;
            return;
        }
        tmp = tmp->nextInAllocationList;
    }
}

MTConfig *consumeMT() {
    if(primaryConfigList == NULL)
        return NULL;
    MTConfig *removed = primaryConfigList;
    primaryConfigList = primaryConfigList->next;
    removed->next = NULL;
    return removed;
}

bool isAcceptation(MTConfig *pConfig) {
    return stateTransitionIndexer[pConfig->state_num] != NULL && stateTransitionIndexer[pConfig->state_num][ACC_INDEX] != NULL;
}

bool executeTransition(MTConfig *currConfig, strict_transition * currentTransition){
    MTConfig *new = NULL;
    //currentTransition->next != NULL è la condizione che dice se una transizione è deterministica o non
    //o meglio, se una transizione è seguita da un'altra, significa che la configurazione corrente
    //mi servirà intonsa anche al prossimo passo, quindi devo copiare la configurazione corrente
    //ed operare sulla copia.
    //Questo controllo consente inoltre di ottimzzare l'uso della memoria, dato che se mi accorgo che
    //la configurazione corrente non mi servirà più in futuro, posso modificarla direttamente.
    if(currentTransition->next != NULL){
        new = copyMTConfig(currConfig);
    }
    else{
        new = currConfig;
    }
    //rimpiazzo il carattere nel nastro
    new->tape[new->index] = (currentTransition)->write;
    //muovo la testina nella direzione specificata dalla transizione
    moveHead((currentTransition)->direction,&new);
    //aggiorno lo stato corrente
    new->state_num = currentTransition->next_state;
    //inserisco la nuova configurazione nella lista stati secondaria
    new->next = NULL;
    produceMT(&new);
    //penso che didSomething non serva neanche più a questo punto, però per essere sicuro lo lascio
    didSomething = true;
    return true;
}

void produceMT(MTConfig **pState) {
    (*pState)->next = secondaryConfigList;
    secondaryConfigList = (*pState);
}

void moveHead(char direction, MTConfig **currConfig) {
    if((*currConfig)->tape == (*currConfig)->rightTape){
        //sono nel nastro di destra
        switch (direction){
            case 'R':
                if((*currConfig)->index == (*currConfig)->rightTapeLength-1) {
                        //se la transizione impone di muovere a destra e il nastro è giunto alla fine
                        //rialloco il puntatore al nastro di una dimensione standard
                    int oldSize = (*currConfig)->rightTapeLength;
                    int newSize = (*currConfig)->rightTapeLength + DEFAULT_INPUT_BLOCK_LENGTH;
                    (*currConfig)->rightTape = realloc((*currConfig)->rightTape, (size_t) newSize);
                    for (int i = oldSize; i < newSize; ++i)
                        (*currConfig)->rightTape[i] = BLANK;
                    (*currConfig)->rightTapeLength = newSize;
                    (*currConfig)->tape = (*currConfig)->rightTape;
                }
                (*currConfig)->index++;
                break;
            case 'L':
                if((*currConfig)->index == 0)
                    (*currConfig)->tape = (*currConfig)->leftTape; //se necessario switcho il nastro
                else
                    (*currConfig)->index--;                         //altrimenti mi limito a decrementare l'indice
                break;
            default:
                break;
        };
    }else{
        //sono nel nastro di sinistra
        //il comportamento è identico al branch vero dell'if, e anche il codice è praticamente uguale
        switch (direction){
            case 'L':
                if((*currConfig)->index == (*currConfig)->leftTapeLength-1) {
                    int oldSize = (*currConfig)->leftTapeLength;
                    int newSize = (*currConfig)->leftTapeLength + DEFAULT_INPUT_BLOCK_LENGTH;
                    (*currConfig)->leftTape = realloc((*currConfig)->leftTape, (size_t) newSize);
                    for (int i = oldSize; i < newSize; ++i)
                        (*currConfig)->leftTape[i] = BLANK;
                    (*currConfig)->leftTapeLength = newSize;
                    (*currConfig)->tape = (*currConfig)->leftTape;
                }
                (*currConfig)->index++;
                break;
            case 'R':
                if((*currConfig)->index == 0)
                    (*currConfig)->tape = (*currConfig)->rightTape;
                else
                    (*currConfig)->index--;
                break;
            default:
                break;
        };
    }
}

MTConfig *copyMTConfig(MTConfig *toCopy) {
    MTConfig *new = malloc(sizeof(MTConfig));
    new->state_num = toCopy->state_num;
    new->leftTape = copyTape(toCopy->leftTape,toCopy->leftTapeLength);
    new->rightTape = copyTape(toCopy->rightTape,toCopy->rightTapeLength);
    new->leftTapeLength = toCopy->leftTapeLength;
    new->rightTapeLength = toCopy->rightTapeLength;

    if(toCopy->tape == toCopy->rightTape)
        new->tape = new->rightTape;
    else
        new->tape = new->leftTape;

    new->index = toCopy->index;
    new->next = NULL;

    //inserisco nella lista di MT allocate, che freeo alla fine dell'esecuzione
    new->nextInAllocationList = *listaMTAllocate;
    *listaMTAllocate = new;

    return new;
}

char *copyTape(const char* tapeToCopy, int length) {
    char* new = calloc((size_t) length, sizeof(char));
    for (int i = 0; i < length; ++i)
        new[i] = tapeToCopy[i];
    return new;
}

char* acquireInputString() {
    char *input;
    if(scanf("%ms",&input) == EOF)  {
        file_finished = true;
        return NULL;
    }

    int length = (int) strlen(input);
    int nextLength = length + DEFAULT_INPUT_BLOCK_LENGTH;
    input = realloc(input, (size_t) nextLength);
    for (int i = length; i < nextLength; ++i)
        input[i] = BLANK;
    return input;
}

void getMaxSteps() {
    char* input = malloc(sizeof(long));
    scanf("%s",input);
    max_steps = strtol(input, NULL, 10);
    free(input);
}


void getAcceptation() {

    char *acc = malloc(sizeof(char)*10);
    scanf("%s",acc);
    int acc_state_num = 0;
    while (strcmp(acc,"max") != 0 && strcmp(acc,"\rmax") != 0 &&strcmp(acc,"max\r") != 0) {

        acc_state_num = atoi(acc);

        //questo posso farlo perchè da specifiche gli stati di accettazione non hanno transizioni uscenti
        stateTransitionIndexer[acc_state_num] = calloc(STRICT_TRANSITIONS_ARRAY_LENGHT, sizeof(strict_transition*));
        for (int i = 0; i < STRICT_TRANSITIONS_ARRAY_LENGHT; ++i)
            stateTransitionIndexer[acc_state_num][i] = NULL;

        //inserisco nel vettore, però, una transizione fittizia ad un indice speciale
        //questa cosa si comporta poi come un flag che indica se uno stato è di accettazione o no
        stateTransitionIndexer[acc_state_num][ACC_INDEX] = malloc(sizeof(strict_transition));
        //avere in posizione ACC_INDEX una finta transizione con direzione della testina 'A' significa essere stato di accetazione
        stateTransitionIndexer[acc_state_num][ACC_INDEX]->write = '@';
        stateTransitionIndexer[acc_state_num][ACC_INDEX]->next  = NULL;
        stateTransitionIndexer[acc_state_num][ACC_INDEX]->direction = ACC_DIRECTION_FLAG;
        stateTransitionIndexer[acc_state_num][ACC_INDEX]->next_state = -1;

        scanf("%s",acc);
    }
    free(acc);

    //------------DEBUG: STAMPA DI TUTTI I VETTORI DI TUTTO IL VETTORE INDICIZZATORE-----------------
    //strict_transition *s = NULL;
    //for (int i = 0; i < max_state; ++i) {
    //    for (int j = 0; j < STRICT_TRANSITIONS_ARRAY_LENGHT; ++j) {
    //        if(stateTransitionIndexer[i] == NULL)
    //            break;
    //        s = stateTransitionIndexer[i][j];
    //        if(s != NULL)
    //            printf("\n------------------------LISTA----------------------------");
    //        while (s != NULL){
    //            printf("\nStato: %d, Write: %c, Direction: %c Next State: %d",i,s->write,s->direction,s->next_state);
    //            s = s->next;
    //        }
    //        //printf("\n-------------------------------------------------------------");
    //    }
    //}
    //-----------------------------------------------------------------------------------------------
}

void groupTransitions() {

    int actual_max_state = max_state+1;
    //creo il vettore incizzatore delle transizioni
    stateTransitionIndexer = calloc((size_t) actual_max_state, sizeof(strict_transition**));
    for (int i = 0; i < actual_max_state; ++i)
        stateTransitionIndexer[i] = NULL;
    //nuovo elemento da inserire nella lista
    strict_transition* newElement = NULL;
    //scorro la lista delle transizioni
    list_type *p = transitions_list_head;
    list_type *temp = NULL;

    while (p != NULL){
        temp = (list_type*) p->linked_state;
        if(stateTransitionIndexer[p->transition->state] == NULL){
            stateTransitionIndexer[p->transition->state] = calloc(STRICT_TRANSITIONS_ARRAY_LENGHT, sizeof(strict_transition*));
            for (int i = 0; i < STRICT_TRANSITIONS_ARRAY_LENGHT; ++i)
                stateTransitionIndexer[p->transition->state][i] = NULL;
        }
        //inserisco la nuova transizione nella lista corrispondente, sempre in testa
        newElement = malloc(sizeof(strict_transition));
        newElement->write = p->transition->write;
        newElement->direction = p->transition->direction;
        newElement->next_state = p->transition->next_state;
        newElement->next = stateTransitionIndexer[p->transition->state][hash_array[(int)p->transition->read]];
        stateTransitionIndexer[p->transition->state][hash_array[(int)p->transition->read]] = newElement;
        free(p);
        p = temp;
    }

    ////------------DEBUG: STAMPA DI TUTTI I VETTORI DI TUTTO IL VETTORE INDICIZZATORE-----------------
    //strict_transition *s = NULL;
    //for (int i = 0; i < max_state; ++i) {
    //    for (int j = 0; j < STRICT_TRANSITIONS_ARRAY_LENGHT; ++j) {
    //        if(stateTransitionIndexer[i] == NULL)
    //            break;
    //        s = stateTransitionIndexer[i][j];
    //        if(s != NULL)
    //            printf("\n------------------------LISTA----------------------------");
    //        while (s != NULL){
    //            printf("\nStato: %d, Read: %d Write: %c, Direction: %c Next State: %d",i,j,s->write,s->direction,s->next_state);
    //            s = s->next;
    //        }
    //        printf("\n-------------------------------------------------------------");
    //    }
    //}
    ////-----------------------------------------------------------------------------------------------
}

void getTransitions() {
    char* state = calloc(31, sizeof(char));  //alloco spazio per l'input
    scanf("%[^\n]%*c", state);              //leggo la prima stringa di tutte, che dovrebbe sempre essere "tr"
    scanf("%[^\n]%*c", state);              //leggo la prima transizione

    list_type* newListElement = NULL;              //puntatore al nuovo elemento della lista
    transition_type* newTransition = NULL;         //pintatore alla nuova transizione creata
    char* token = NULL;
    char acc[4] = "acc";
    while(strcmp(state,acc) != 0 && strcmp(state,"\racc") != 0 &&strcmp(state,"acc\r") != 0){        //eseguo finchè non leggo "acc"
        newListElement = malloc(sizeof(list_type));         //alloco spazio per il nuovo elemento della lista delle transizioni
        newTransition = malloc(sizeof(transition_type));    //alloco spazio per la nuova transizione
        token = strtok(state," ");
        newTransition->state = atoi(token);   //leggo lo stato come un int
        token = strtok(NULL," ");
        newTransition->read = *token;                   //leggo read come char
        token = strtok(NULL," ");
        newTransition->write = *token;                  //leggo write come char
        token = strtok(NULL," ");
        newTransition->direction = *token;              //leggo la direzione come char
        token = strtok(NULL," ");
        newTransition->next_state = atoi(token);  //leggo lo stato prossimo come int

        newListElement->transition = newTransition;
        newListElement->linked_state = (struct list_type*) transitions_list_head;
        transitions_list_head = newListElement;

        if(max_state < newTransition->state)
            max_state = newTransition->state;
        if(max_state < newTransition->next_state)
            max_state = newTransition->next_state;

        scanf("%[^\n]%*c", state);
    }
    free(state);
}

void freeOldList(MTConfig **pState) {
    MTConfig* temp = NULL;
    while ((*pState) != NULL){
        temp = (*pState)->next;
        removeFromListaAllocazione((*pState));
        free((*pState)->leftTape);
        free((*pState)->rightTape);
        (*pState)->leftTapeLength = -1;
        free((*pState));
        (*pState) = temp;
    }
}


