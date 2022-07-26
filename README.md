# Modalità d'uso

Per poter runnare il progetto, o qualsiasi test, è necessario prima caricare le variabili run-time nell'environment.

Per far questo, sono stati creati 3 file di configurazione (più uno di template - default.env)

- conf1.env
- conf2.env
- conf3.env

In caso si voglia provare una configurazione customizzata, creare un file e chiamarlo .env. Poi, utilizzare questo comando
nella shell che si vuole usare

```bash
export $(xargs <.env)
```

In questo modo tutte le variabili in .env verranno caricate nelle variabili d'ambiente.
