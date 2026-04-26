#pragma once
#include <SFML/Graphics.hpp>
#include <functional>

#include "Common/ComponentType.h"
#include "Common/SimulationOutput.h"
#include "UI/ComponentView.h"

namespace CircuitLab {

	// Stato della selezione corrente nel canvas:
	// niente selezionato, un componente selezionato, o un terminale selezionato.
	enum class SelectionState {
		none,
		componentSelected,
		terminalSelected
	};

	// Tiene traccia del componente (o terminale) attualmente selezionato.
	// Usato per gestire il doppio click sui terminali per creare un collegamento.
	struct SelecetedComponent {
		int compId;          // ID del componente selezionato (-1 se nessuno)
		int terminalIndex;   // Indice del terminale selezionato (-1 se nessuno o se è il corpo)
		SelectionState state;
	};

	// Rappresentazione visiva di un collegamento tra due terminali:
	// semplicemente i due punti estremi del filo nel canvas,
	// con i riferimenti ai componenti collegati (usati per rimuovere il filo
	// quando un componente viene eliminato).
	struct LinkView {
		sf::Vector2f pointA;
		sf::Vector2f pointB;
		int compIdA;   // ID del componente sul primo estremo del filo
		int compIdB;   // ID del componente sul secondo estremo del filo
		int termIndexA; // Indice del terminale del componente A
		int termIndexB; // Indice del terminale del componente B
	};

	// Classe principale dell'interfaccia grafica.
	// Gestisce la finestra SFML, il loop degli eventi, il rendering dei componenti
	// e dei collegamenti, e il pannello ImGui.
	// Comunica con Application tramite cinque callback:
	//   - m_onRunSimulation:      avvia la simulazione e restituisce i risultati
	//   - m_onCircuitChange:      aggiunge un componente al circuito, restituisce il suo ID
	//   - m_onCreateLink:         collega due terminali nel circuito
	//   - m_onGetCompTerminalId:  richiede i nodeId dei terminali di un componente
	//   - m_onDeleteComponent:    rimuove un componente dal circuito
	class UI {
	public:
		// Callback per aggiungere un componente: riceve tipo e valore, restituisce l'ID assegnato
		using fnCircuitChange = std::function<int(CircuitLab::ComponentType type, double value)>;
		// Callback per collegare due terminali: restituisce true se il collegamento è andato a buon fine
		using fnCreateLink = std::function<bool(int compId1, int termIndex1, int compId2, int termIndex2)>;
		// Callback per ottenere i nodeId dei terminali di un componente
		using fnGetCompTerminalId = std::function<std::vector<int>(int compId)>;
		// Callback per eliminare un componente dal circuito
		using fnDeleteComponent = std::function<void(int compId)>;

	private:
		unsigned int m_width;   // Larghezza della finestra (pixel)
		unsigned int m_heigth;  // Altezza della finestra (pixel)
		std::string m_title;    // Titolo della finestra
		sf::Font m_font;        // Font usato per le etichette dei componenti sul canvas

		// Costanti di configurazione UI
		static constexpr int CLICK_TOLLERANCE = 5;         // Tolleranza click sui terminali (pixel)
		static constexpr double DEFAULT_RESISTANCE = 1.0;  // Resistenza di default (Ohm)
		static constexpr double DEFAULT_VOLTAGE = 5.0;     // Tensione di default (Volt)
		static constexpr float DEFAULT_ROTATION = 0.0f;    // Rotazione di default (gradi)
		static constexpr int OUTLINE_THICKNESS = 2;        // Spessore outline selezione (pixel)
		static constexpr int TEXT_COMPONENT_OFFSET = 15;   // Offset orizzontale etichetta rispetto al centro del componente (pixel)
		inline static const sf::Color BACKGROUND_COLOR = sf::Color(30, 30, 30); // Colore sfondo canvas

		SimulationOutput m_simulationOutput;  // Ultimo risultato di simulazione ricevuto

		SelecetedComponent m_selectedComponent; // Componente/terminale attualmente selezionato

		std::vector<ComponentView> m_componentViewList; // Lista delle viste grafiche dei componenti
		std::vector<LinkView> m_linkViewList;           // Lista dei collegamenti (fili) da disegnare

		sf::RenderWindow m_window; // Finestra SFML

		// Callback impostati da Application
		std::function<SimulationOutput()> m_onRunSimulation;
		fnCircuitChange m_onCircuitChange;
		fnCreateLink m_onCreateLink;
		fnGetCompTerminalId m_onGetCompTerminalId; // Richiede i nodeId dei terminali al circuito
		fnDeleteComponent m_onDeleteComponent;     // Richiede la rimozione di un componente al circuito

		// Determina quale componente o terminale è stato cliccato nella posizione pos.
		// Aggiorna selComp con il risultato.
		void CheckClick(sf::Vector2i pos, SelecetedComponent &selComp);

		// Calcola le coordinate pixel dei due estremi di un collegamento,
		// tenendo conto delle posizioni e degli offset dei terminali.
		LinkView GetLinkCoords(int comp1, int term1, int comp2, int term2);

		// Calcola la posizione ruotata di un terminale nel canvas,
		// tenendo conto della rotazione del componente.
		sf::Vector2f GetRotatedTermnialPos(const ComponentView &cw, int termIndex);

		// Aggiorna le coordinate dei fili collegati a un componente
		// dopo che quest'ultimo è stato ruotato o spostato.
		void UpdateLinksForComponent(int compId);

	public:
		UI(unsigned int width, unsigned int heigth, const std::string &title);
		~UI();

		// Setter per i callback - chiamati da Application nel costruttore
		void SetOnRunSimulation(const std::function<SimulationOutput()> &func) { m_onRunSimulation = func; }
		void SetOnCircuitChange(const fnCircuitChange &func) { m_onCircuitChange = func; }
		void SetOnCreateLink(const fnCreateLink &func) { m_onCreateLink = func; }
		void SetOnGetCompTerminalId(const fnGetCompTerminalId &func) { m_onGetCompTerminalId = func; }
		void SetOnDeleteComponent(const fnDeleteComponent &func) { m_onDeleteComponent = func; }

		// Avvia il loop principale: gestione eventi, aggiornamento ImGui, rendering
		void Run();
	};
}