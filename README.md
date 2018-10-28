# NonDeterministic-TM-Simulator
Implementazione in linguaggio C standard (con la sola libc) di un interprete di Macchine di Turing non-deterministiche, nella variante a nastro singolo e solo accettori.

Struttura del file di ingresso: prima viene fornita la funzione di transizione, quindi gli stati di accettazione e un limite massimo sul numero di passi da effettuare per una singola computazione (per evitare in maniera banale il problema delle macchine che non terminano), infine una serie di stringhe da far leggere alla macchina.

In uscita ci si attende un file contenente 0 per le stringhe non accettate e 1 per quelle accettate; essendoci anche un limite sul numero di passi, il risultato può anche essere U se non si è arrivati ad accettazione.

## Convenzioni Adottate

Per semplicità i simboli di nastro sono dei char, mentre gli stati sono int. Il carattere "\_" indica il simbolo "blank". La macchina parte sempre dallo stato 0 e dal primo carattere della stringa in ingresso. Si assume che il nastro sia illimitato sia a sinistra che a destra e che contenga in ogni posizione il carattere "\_". I caratteri "L", "R", "S" vengono usati per il movimento della testina. Il file di ingresso viene fornito tramite lo standard input, mentre quello in uscita è sullo standard output.

## Struttura del File d'Ingresso

Il file di ingresso è diviso in 4 parti: 
* La prima parte, che inizia con "tr", contiene le transizioni, una per linea - ogni carattere può essere separato dagli altri da spazi. Per es. 0 a c R 1 significa che si va dallo stato 0 allo stato 1, leggendo a e scrivendo c; la testina viene spostata a destra (R). 
* La parte successiva, che inizia con "acc", elenca gli stati di accettazione, uno per linea. 
* Per evitare problemi di computazioni infinite, nella sezione successiva, che inizia con "max", viene indicato il numero di mosse massimo che si possono fare per accettare una stringa. 
* La parte finale, che inizia con "run", è un elenco di stringhe da fornire alla macchina, una per linea.
