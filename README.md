# Modalità d'uso

## Compilazione

Prima di poter compilare, bisogna creare la cartella `./build`, dove verranno contenuti i file oggetto.

## Variabili run-time

Per poter runnare il progetto è necessario prima caricare le variabili run-time nell'environment.

Per far questo, sono stati creati 3 file di configurazione (più uno di template - default.env)

- conf1.env
- conf2.env
- conf3.env

Attraverso lo script `load_config.sh` è possibile automaticamente caricare le variabili dentro questi 3 file

Es.

```bash
. ./load_config.sh 2
```

carica le variabili contenute in conf2.env dentro l'environment della shell in cui viene runnato.

In caso si voglia provare una configurazione customizzata, creare un file con le variabili corrette.

Un modo per caricare delle variabili nell'environment, contenute in file chiamato .env è il seguente

```bash
export $(xargs <.env)
```
