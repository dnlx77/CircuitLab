# CircuitLab

CircuitLab è un simulatore di circuiti elettrici analogici scritto in C++20 con interfaccia grafica.
Permette di costruire circuiti interattivamente su un canvas, collegare i componenti ed eseguire
una simulazione basata sulla Modified Nodal Analysis (MNA) per calcolare tensioni ai nodi e
correnti nei rami.

## Funzionalità

- Canvas interattivo per posizionare e collegare componenti
- Simulazione nel tempo basata sulla Modified Nodal Analysis (MNA), con modelli companion per i
  componenti dinamici e Newton-Raphson per quelli non lineari
- Componenti supportati: resistore, generatore di tensione (continua, sinusoidale, onda quadra),
  condensatore, induttore, interruttore, deviatore, diodo, transistor bipolare NPN e PNP,
  trasformatore, massa
- Oscilloscopio (ImPlot) con sonde di tensione e di corrente, pallini della corrente sui fili
  e finestra dei risultati in tempo reale
- Salvataggio e caricamento dei circuiti in JSON, annulla e ripeti
- Separazione netta tra logica di simulazione (`CircuitLabCore`) e layer UI

## Struttura del progetto
CircuitLab/
├── include/
│   ├── App/            # Mediator dell'applicazione
│   ├── Common/         # Tipi condivisi (ComponentType, SimulationOutput, Logger)
│   ├── Components/     # Modelli dei componenti (Resistor, Capacitor, Diode, Transistor...)
│   ├── Core/           # Motore MNA (Circuit, Solver, NonlinearSolve, TransientStepper)
│   ├── IO/             # Salvataggio e caricamento JSON (IOManager)
│   └── UI/             # Layer UI (UI, ComponentView, WireGraph)
├── src/
│   ├── app/            # Application.cpp
│   ├── components/     # Implementazioni dei componenti
│   ├── core/           # Circuit.cpp, Solver.cpp, NonlinearSolve.cpp, TransientStepper.cpp
│   ├── io/             # IOManager.cpp
│   └── ui/             # UI.cpp, ComponentView.cpp, WireGraph.cpp, FileDialog.cpp
├── CircuitLabCore/     # Libreria statica del nucleo di simulazione (core/, components/, Logger)
├── tests/              # Test automatici (Catch2) del nucleo di simulazione
├── external/           # Librerie di terze parti (submodules)
│   ├── SFML/
│   ├── imgui/
│   ├── imgui-sfml/
│   ├── implot/
│   ├── eigen/
│   ├── nlohmann-json/
│   └── catch2/
├── lib/                # Librerie SFML precompilate
├── assets/             # Font dell'interfaccia
├── CircuitLab/         # File di progetto Visual Studio dell'applicazione
└── CircuitLab.slnx     # File di soluzione Visual Studio

## Dipendenze

| Libreria | Versione | Utilizzo |
|---|---|---|
| [SFML](https://www.sfml-dev.org/) | 3.0.2 | Finestra, rendering, eventi |
| [ImGui](https://github.com/ocornut/imgui) | 1.91.9 | Pannelli GUI |
| [imgui-sfml](https://github.com/SFML/imgui-sfml) | latest | Backend ImGui per SFML (compilato dai sorgenti) |
| [ImPlot](https://github.com/epezent/implot) | latest | Grafici dell'oscilloscopio |
| [Eigen](https://eigen.tuxfamily.org/) | 3.x (header-only) | Algebra lineare (solver MNA) |
| [nlohmann-json](https://github.com/nlohmann/json) | 3.x | Salvataggio/caricamento JSON |
| [Catch2](https://github.com/catchorg/Catch2) | 3.16.1 (single header, in `external/catch2`) | Test automatici |

## Per iniziare

### Prerequisiti

- Windows 10/11
- Visual Studio con supporto C++20 e ai file di soluzione `.slnx`
- Git (per clonare i submodule)

### Clonare il repository

```bash
git clone https://github.com/dnlx77/CircuitLab.git
cd CircuitLab
git submodule update --init --recursive
```

### Compilazione

1. Aprire `CircuitLab.slnx` con Visual Studio
2. Selezionare la configurazione `Debug | x64` oppure `Release | x64`
3. Compilare la soluzione (`Ctrl+Shift+B`)

> **Nota:** Le librerie precompilate di SFML devono essere collocate in `lib/SFML/`
> (le DLL in `lib/SFML/bin/`), rispettando la struttura delle sottocartelle `Debug/` e
> `Release/` attesa dal progetto. imgui-sfml e ImPlot si compilano dai sorgenti in `external/`.

### Test

I test (Catch2) coprono il nucleo di simulazione, senza interfaccia grafica: modello del
transistor NPN/PNP in tutte le regioni (confrontato con una soluzione indipendente delle
equazioni di Ebers-Moll), convergenza di Newton su circuiti difficili (trigger di Schmitt,
amplificatore, multivibratore astabile), punto di lavoro DC e circuiti lineari.

```bash
msbuild CircuitLab.slnx /p:Configuration=Release /p:Platform=x64
bin\tests\Release\CircuitLabTests.exe
```

Il nucleo (circuito, solver, componenti) è la libreria statica `CircuitLabCore`, referenziata
sia dall'applicazione sia dai test: i sorgenti sono elencati e compilati una volta sola. Si può
compilare anche solo `tests/CircuitLabTests.vcxproj`, che non richiede SFML. Conviene lanciare i
test in `Release`: in `Debug` Eigen non è ottimizzato e la suite impiega alcuni minuti invece di
qualche secondo. Opzioni utili: `CircuitLabTests.exe "[transistor]"` esegue solo i test con quell'etichetta
(`[solver]`, `[dc]`, `[circuit]`...), `--list-tests` li elenca. Ogni bug corretto dovrebbe
lasciare un test che lo riproduce.

Per misurare le prestazioni del passo di simulazione c'è anche un benchmark, nascosto dalla suite
normale: `CircuitLabTests.exe "[bench]"` stampa i microsecondi per passo su alcuni circuiti
(RC lineari, amplificatore, trigger di Schmitt, multivibratore, scala di diodi). Da lanciare in
`Release` prima e dopo una modifica al solver o ai componenti.

### Utilizzo

L'interfaccia ha una barra dei menu in alto, una barra degli strumenti con i comandi di
simulazione e una barra di stato in fondo; il canvas occupa tutto il resto.

| Menu | Voci |
|---|---|
| **File** | Nuovo (`Ctrl+N`), Apri (`Ctrl+O`), Salva (`Ctrl+S`), Salva con nome (`Ctrl+Maiusc+S`), Esci |
| **Modifica** | Annulla (`Ctrl+Z`), Ripeti (`Ctrl+Y`), Elimina selezione (`Canc`), Ruota di 45 gradi (`Q`), Specchia (`F`) |
| **Componenti** | Tutti i componenti per categoria: si sceglie una voce, il simbolo segue il mouse e ogni click lo posa; `Esc` o click destro per finire |
| **Simulazione** | Avvia (`F5`), Pausa (`F6`), Stop (`F7`), passo di tempo, velocita', partenza dal punto di lavoro DC |
| **Visualizza** | Oscilloscopio, finestra Risultati, pallini della corrente e loro velocita', griglia, aggancio e passo della griglia, reset dello zoom |

La barra degli strumenti ripete Avvia/Pausa/Stop e i selettori di passo di tempo e velocita'.

Sul canvas:

| Input | Azione |
|---|---|
| `R`, `C`, `L`, `V`, `G`, `S`, `Y`, `D`, `J`, `K`, `T` + click sinistro | Posa subito il componente (resistore, condensatore, induttore, generatore, massa, interruttore, deviatore, diodo, transistor NPN/PNP, trasformatore) |
| `N` + click sinistro | Posa un nodo libero |
| Click sinistro su un terminale | Seleziona il terminale |
| Click sinistro su un secondo terminale | Collega i due terminali |
| Trascinamento col tasto destro | Sposta componenti e nodi |
| Rotella / tasto centrale | Zoom / spostamento della vista |

## Architettura

CircuitLab segue il pattern **Mediator**:

- `Application` possiede sia `Circuit` che `UI` e li collega tramite callback
- `Circuit` gestisce i componenti e costruisce il sistema MNA con lazy evaluation
- `Solver` risolve il sistema lineare `A·x = b` (equilibratura di Ruiz, LU con pivot parziale e ripiego sulla QR a pivot di colonna)
- `UI` gestisce rendering e interazione utente, comunicando con `Application`
  tramite callback `std::function`
- `ComponentView` mantiene i dati visivi (posizione, rotazione) separati dai dati
  di simulazione (`Component`)

## Roadmap

- [ ] File di configurazione per dimensioni finestra e preferenze UI

## Licenza

Questo progetto è distribuito sotto licenza MIT. Consulta il file [LICENSE](LICENSE) per i dettagli.