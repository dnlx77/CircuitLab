# CircuitLab

CircuitLab è un simulatore di circuiti elettrici analogici scritto in C++20 con interfaccia grafica.
Permette di costruire circuiti interattivamente su un canvas, collegare i componenti ed eseguire
una simulazione basata sulla Modified Nodal Analysis (MNA) per calcolare tensioni ai nodi e
correnti nei rami.

## Funzionalità

- Canvas interattivo per posizionare e collegare componenti
- Motore di simulazione basato sulla Modified Nodal Analysis (MNA)
- Componenti supportati: resistenze, sorgenti di tensione, ground
- Risultati visualizzati in tempo reale tramite pannello ImGui
- Separazione netta tra logica di simulazione e layer UI

## Struttura del progetto
CircuitLab/
├── include/
│   ├── App/            # Mediator dell'applicazione
│   ├── Common/         # Tipi condivisi (ComponentType, SimulationOutput)
│   ├── Components/     # Modelli dei componenti (Resistor, VoltageSource, Ground)
│   ├── Core/           # Motore MNA (Circuit, Solver, Vector2)
│   └── UI/             # Layer UI (UI, ComponentView)
├── src/
│   ├── app/            # Application.cpp
│   ├── components/     # Implementazioni dei componenti
│   ├── core/           # Circuit.cpp, Solver.cpp
│   └── ui/             # UI.cpp, ComponentView.cpp
├── external/           # Librerie di terze parti (submodules)
│   ├── SFML/
│   ├── imgui/
│   ├── imgui-sfml/
│   ├── eigen/
│   └── nlohmann-json/
├── lib/                # Librerie precompilate (SFML, imgui-sfml)
├── CircuitLab/         # File di progetto Visual Studio
└── CircuitLab.slnx     # File di soluzione Visual Studio

## Dipendenze

| Libreria | Versione | Utilizzo |
|---|---|---|
| [SFML](https://www.sfml-dev.org/) | 3.0.2 | Finestra, rendering, eventi |
| [ImGui](https://github.com/ocornut/imgui) | 1.91.9 | Pannelli GUI |
| [imgui-sfml](https://github.com/SFML/imgui-sfml) | latest | Backend ImGui per SFML |
| [Eigen](https://eigen.tuxfamily.org/) | 3.x (header-only) | Algebra lineare (solver MNA) |
| [nlohmann-json](https://github.com/nlohmann/json) | 3.x | Salvataggio/caricamento JSON (pianificato) |

## Per iniziare

### Prerequisiti

- Windows 10/11
- Visual Studio 2022 con supporto C++20
- Git (per clonare i submodule)

### Clonare il repository

```bash
git clone https://github.com/dnlx77/CircuitLab.git
cd CircuitLab
git submodule update --init --recursive
```

### Compilazione

1. Aprire `CircuitLab.slnx` con Visual Studio 2022
2. Selezionare la configurazione `Debug | x64` oppure `Release | x64`
3. Compilare la soluzione (`Ctrl+Shift+B`)

> **Nota:** Le librerie precompilate di SFML e imgui-sfml devono essere collocate
> rispettivamente in `lib/SFML/` e `lib/imgui-sfml/`, rispettando la struttura
> delle sottocartelle `Debug/` e `Release/` attesa dal progetto.

### Utilizzo

| Input | Azione |
|---|---|
| `R` + click sinistro | Posiziona una resistenza |
| `V` + click sinistro | Posiziona una sorgente di tensione |
| `G` + click sinistro | Posiziona un nodo di ground |
| Click sinistro su un terminale | Seleziona il terminale |
| Click sinistro su un secondo terminale | Collega i due terminali |
| Pulsante `RunSimulation` | Esegue la simulazione MNA e mostra i risultati |

## Architettura

CircuitLab segue il pattern **Mediator**:

- `Application` possiede sia `Circuit` che `UI` e li collega tramite callback
- `Circuit` gestisce i componenti e costruisce il sistema MNA con lazy evaluation
- `Solver` risolve il sistema lineare `A·x = b` tramite decomposizione QR
- `UI` gestisce rendering e interazione utente, comunicando con `Application`
  tramite callback `std::function`
- `ComponentView` mantiene i dati visivi (posizione, rotazione) separati dai dati
  di simulazione (`Component`)

## Roadmap

- [ ] Salvataggio e caricamento dei circuiti in JSON
- [ ] Rotazione dei componenti sul canvas
- [ ] Spostamento dei componenti tramite drag
- [ ] Eliminazione di componenti e collegamenti
- [ ] Componenti non lineari (diodi, transistor) tramite Newton-Raphson
- [ ] File di configurazione per dimensioni finestra e preferenze UI

## Licenza

Questo progetto è distribuito sotto licenza MIT. Consulta il file [LICENSE](LICENSE) per i dettagli.