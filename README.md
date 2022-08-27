# Modalità d'uso

## Compilazione

Prima di poter compilare, bisogna creare la cartella `./build`, dove verranno contenuti i file oggetto.

## Variabili run-time

Per poter runnare il progetto è necessario prima caricare le variabili run-time nell'environment.

Per far questo, sono stati creati 3 file di configurazione (più uno di template - default.env)

- conf1.env
- conf2.env
- conf3.env

Attraverso lo script `init_environment.sh` è possibile automaticamente caricare le variabili dentro questi 3 file

Es.

```bash
. ./init_environment.sh conf2.env
```

carica le variabili contenute in conf2.env dentro l'environment della shell in cui viene runnato.

In caso si voglia provare una configurazione customizzata, creare un file con le variabili corrette ed passarlo come paramentro allo script sopra citato.

## Variabili compile-time

È possibile specificare al compilatore quale configurazione si vuole runnare attraverso gli appositi target `make conf1`, `make conf2`, `make conf3`. Questi target caricano una macro che permette di scegliere le variabili corrette (vedere Makefile e ./src/constants/constants.h). In caso si voglia invece provare una configurazione custom, modificare in src/constants/constants.h le variabili e runnare `make run.out` o semplicemente `make`.
