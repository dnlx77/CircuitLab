#pragma once
#include <array>
#include <map>
#include <string>
#include <vector>
#include <Eigen/Dense>
#include <nlohmann/json.hpp>
#include "Terminal.h"
#include "Stamp.h"
#include "Common/ComponentType.h"
#include "Common/ComponentValue.h"

namespace CircuitLab {

	// Conduttanza (in Siemens) con cui si approssima un cortocircuito ideale:
	// interruttore chiuso, resistore/induttore di valore quasi nullo. Vale 1 µΩ.
	// Non va alzata "per essere più ideale": la soluzione lineare (QR in double)
	// ha un rumore di arrotondamento proporzionale al termine più grande della
	// matrice, e con 1e12 S l'errore sulle tensioni arrivava a decine di mV, fino
	// a soluzioni prive di senso quando un condensatore grande (Geq = C/h) fa
	// crescere ancora l'intervallo dinamico. Con 1e6 S l'errore scende a ~1e-8 V,
	// mentre la caduta sul "filo" resta trascurabile (1 A -> 1 µV).
	inline constexpr double SHORT_CIRCUIT_CONDUCTANCE = 1e6;

	// Valore speciale del passo h (sia in StampMatrix sia in StampContext::h) con cui
	// Circuit chiede il modello del PUNTO DI LAVORO DC, cioè il regime stazionario con
	// tutte le grandezze costanti: condensatori aperti, induttori (e avvolgimenti dei
	// trasformatori) in corto, generatori al loro valore continuo, nessuna memoria dei
	// passi precedenti. Gli altri componenti (resistori, diodi, transistor...) non
	// dipendono da h e si stampano come sempre.
	inline constexpr double DC_ANALYSIS_STEP = -1.0;

	// Contesto passato a StampVector ad ogni step di simulazione:
	// t = tempo corrente, h = passo di integrazione.
	struct StampContext {
		double t = 0.0;
		double h = 0.0;
	};

	// Correnti di un componente alla soluzione appena risolta, come le vuole la UI:
	//  - main: il valore del probe "corrente di componente" dell'oscilloscopio (uno solo
	//    per componente);
	//  - branches: le correnti dei singoli rami, che la UI usa per animare i fili. Un ramo
	//    va dal terminale `from` al terminale `to` (indici in GetTerminals()); con
	//    to == SINGLE_TERMINAL è la corrente del solo terminale `from` (transistor, dove
	//    le tre correnti sono indipendenti e non c'è una coppia di capi).
	// Al massimo 3 rami (il deviatore e il transistor): niente allocazioni per step.
	struct CurrentReport {
		static constexpr int SINGLE_TERMINAL = -1;
		struct Branch { int from; int to; double current; };

		double main = 0.0;
		std::array<Branch, 3> branches{};
		int branchCount = 0;

		void AddBranch(int from, int to, double current) { branches[branchCount++] = { from, to, current }; }
	};

	class Circuit;

	// Classe base astratta per tutti i componenti elettrici.
	// Ogni componente ha un ID univoco, un certo numero di terminali
	// e contribuisce alla matrice MNA con StampMatrix/StampVector (o StampNonlinear).
	class Component {
		friend class Circuit; // assegna l'id in AddComponent

	protected:
		int m_id = 0;               // ID univoco nel circuito, assegnato da Circuit::AddComponent
		std::vector<Terminal> m_terminals; // Lista dei terminali
		ComponentType m_componentType;

		// Metodi per la serializzazione - da implementare nelle classi derivate
		virtual void SaveSpecificData(nlohmann::json &j) const = 0;
		virtual void LoadSpecificData(const nlohmann::json &j) = 0;

		// Indice di riga/colonna nella matrice MNA del nodo a cui è collegato il terminale
		// dato (-1 se è a massa o non collegato: non ha riga e non si scrive nulla)
		int MatrixIndex(int terminal, const std::map<int, int> &nodeMap) const
		{
			const int nodeId = m_terminals[terminal].GetNodeId();
			return (nodeId > 0) ? nodeMap.at(nodeId) : -1;
		}

	public:
		// Il costruttore alloca automaticamente il numero corretto di terminali
		Component(int terminalNumber, ComponentType compType) : m_componentType(compType) {
			m_terminals.resize(terminalNumber);
		}
		virtual ~Component() = default;

		int GetId() const { return m_id; }
		const std::vector<Terminal> &GetTerminals() const { return m_terminals; }
		Terminal &GetTerminal(int index) { return m_terminals[index]; }
		// Restituisce il nodeId di ogni terminale del componente (non l'ID del terminale).
		std::vector<int> GetTerminalNodeIds() const;
		virtual bool IsGround() const { return false; }
		ComponentType GetType() const { return m_componentType; }

		// Save/Load scrivono/leggono solo id, type e i dati specifici del componente
		// (delegati a SaveSpecificData/LoadSpecificData). Il remapping degli ID
		// tra istanze diverse è gestito a monte da IOManager, non qui.
		void Save(nlohmann::json &j) const;
		void Load(const nlohmann::json &j);

		// Stampa il contributo del componente nella matrice MNA (A) e nel vettore (b).
		// nodeMap mappa nodeId -> indice di riga/colonna nella matrice.
		// voltageSourceMap mappa componentId -> indice della riga extra per le sorgenti di tensione.
		// Di default non contribuiscono a nulla (massa, componenti non lineari per la parte
		// statica, componenti resistivi per quella dinamica): ogni componente sovrascrive
		// solo la parte che gli serve. Per scrivere in A e b vedi Stamp.h e MatrixIndex().
		//
		// StampMatrix: parte statica (dipendente da topologia e passo di simulazione h),
		// chiamata una sola volta, quando la matrice viene (ri)fattorizzata.
		// h è necessario ai componenti con modello companion (es. condensatori/induttori),
		// la cui conduttanza equivalente dipende dal passo di integrazione.
		virtual void StampMatrix(Eigen::MatrixXd &, const std::map<int, int> &, const std::map<int, int> &, double) {}

		// StampVector: parte dinamica, richiamata ad ogni step di simulazione
		// (dipende da ctx.t, quindi va aggiornata ad ogni istante).
		virtual void StampVector(Eigen::VectorXd &, const std::map<int, int> &, const std::map<int, int> &, const StampContext &) {}

		// True per i componenti la cui corrente non è lineare nella tensione (es. il
		// diodo): non possono essere stampati una volta sola in A, quindi la
		// simulazione deve risolvere ogni step con iterazioni di Newton-Raphson
		// (vedi SolveNonlinearStep in NonlinearSolve.h). Default: componente lineare.
		virtual bool IsNonlinear() const { return false; }

		// Solo per i componenti non lineari: linearizza il componente attorno alla
		// soluzione x dell'iterazione precedente di Newton e stampa il modello
		// companion risultante (conduttanza in A, generatore equivalente in B).
		// Restituisce true se il componente ha dovuto limitare la propria tensione
		// (in quel caso l'iterazione non può essere considerata convergita).
		// Default: no-op, restituisce false.
		virtual bool StampNonlinear(Eigen::MatrixXd &, Eigen::VectorXd &, const std::map<int, int> &, const Eigen::VectorXd &)
		{
			return false;
		}

		// Solo per i componenti non lineari: dopo aver risolto il sistema con la
		// linearizzazione stampata da StampNonlinear, verifica se la soluzione x è
		// coerente col componente VERO — cioè se la corrente predetta dal modello
		// linearizzato coincide (entro tolleranza) con quella reale nel nuovo punto.
		// È il test di convergenza di SPICE: a differenza del confronto tra due
		// soluzioni successive non viene falsato dal rumore numerico dei nodi
		// quasi isolati. Default: true.
		virtual bool HasConverged(const std::map<int, int> &, const Eigen::VectorXd &) const
		{
			return true;
		}

		// Restituisce il numero di variabili extra introdotte nella matrice MNA.
		// Di default 0; le sorgenti di tensione lo sovrascrivono con 1
		// (la corrente incognita che scorre nel generatore).
		virtual int GetExtraVariables() const { return 0; }

		// True per i componenti con memoria di stato (condensatore, induttore,
		// trasformatore): dopo ogni step risolto TransientStepper chiama il loro
		// UpdateState. Gli altri non ne hanno bisogno e si saltano.
		virtual bool HasMemory() const { return false; }

		// Aggiorna lo stato interno del componente con le tensioni ai suoi terminali
		// risolte allo step corrente (terminalVoltages: una per terminale, nello stesso
		// ordine di GetTerminals(), rispetto a massa). Usato dai componenti con memoria
		// (es. condensatori: serve la tensione del passo precedente per il modello
		// companion). Default: no-op.
		virtual void UpdateState(const std::vector<double> &) {}

		// Calcola le correnti del componente alla soluzione appena risolta (terminalVoltages:
		// una per terminale, come in UpdateState; h: passo di integrazione). Va chiamata PRIMA
		// di UpdateState, perché condensatore, induttore e trasformatore usano lo stato
		// del passo precedente. Restituisce false se il componente non ha una corrente da
		// mostrare qui (massa; il generatore di tensione ha la sua incognita nella soluzione).
		virtual bool ReportCurrents(const std::vector<double> &, double, CurrentReport &) const { return false; }

		// Azzera lo stato dinamico accumulato durante la simulazione (tensione/
		// corrente del passo precedente per condensatori/induttori/trasformatore,
		// punto di linearizzazione per diodo/transistor) — NON i valori impostati
		// dall'utente (resistenza, capacità, Is, BF...), quelli restano. Chiamato
		// da Circuit::ResetDynamicState quando cambia il timestep di simulazione
		// (vedi UI::SetOnSetHSim): un passo molto diverso da quello con
		// cui quello stato è stato calcolato può produrre uno step fisicamente
		// legittimo ma numericamente estremo (es. Geq=C/h con h improvvisamente
		// enorme o minuscolo), che poi si porta dietro per molti passi anche
		// tornando al timestep originale. Default: no-op (componenti senza
		// memoria, come Resistor, non hanno nulla da azzerare).
		virtual void ResetDynamicState() {}

		// Imposta lo stato dinamico (tensione del condensatore, corrente dell'induttore...)
		// al valore che ha nel punto di lavoro DC appena calcolato: terminalVoltages sono le
		// tensioni dei terminali del componente, nello stesso ordine di GetTerminals(), rispetto
		// a massa. Da chiamare PRIMA che la matrice venga ristampata con il passo di
		// integrazione (alcuni componenti usano ancora i coefficienti dell'analisi DC).
		// Default: no-op (componenti senza memoria).
		virtual void SetStateFromDc(const std::vector<double> &) {}

		// Inverte lo stato aperto/chiuso. Solo lo Switch lo sovrascrive;
		// per tutti gli altri componenti è un no-op (non ha senso "aprirli").
		virtual void ToggleSwitch() {}

		// Stato aperto/chiuso, usato dalla UI per scegliere il simbolo da disegnare.
		// Solo lo Switch lo sovrascrive; il default true è irrilevante per gli altri tipi.
		virtual bool IsSwitchClosed() const { return true; }

		// Solo i componenti con forma d'onda (es. VoltageGenerator) sovrascrivono questi;
		// gli altri restituiscono WaveFormType::none / non fanno nulla.
		virtual WaveFormType GetWaveFormType() const { return WaveFormType::none; }
		virtual void SetWaveFormType(WaveFormType) {}

		// Valori modificabili dall'utente (resistenza, capacità...), mostrati nel pannello
		// delle proprietà. Di default nessuno (massa, interruttori: lo stato si cambia
		// con un click, non scrivendo un numero).
		virtual std::map<ComponentValue, double> GetValues() const { return {}; }
		virtual void SetValues(const std::map<ComponentValue, double> &) {}
	};
}