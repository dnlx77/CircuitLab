#pragma once
#include <SFML/Graphics.hpp>
#include <functional>

#include "Common/ComponentType.h"
#include "Common/SimulationOutput.h"
#include "UI/ComponentView.h"
#include "Common/UICommon.h"

namespace CircuitLab {

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

		using fnOnSave = std::function<void(const std::string &filePath)>; // Callback per salvare il circuito su file
		using fnOnLoad = std::function<void(const std::string &filePath)>; // Callback per caricare il circuito da file
		using fnOnNew = std::function<void()>;                             // Callback per resettare il canvas

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

		sf::Vector2f m_compClickOffset;

		sf::RenderWindow m_window; // Finestra SFML

		// Callback impostati da Application
		std::function<SimulationOutput()> m_onRunSimulation;
		fnCircuitChange m_onCircuitChange;
		fnCreateLink m_onCreateLink;
		fnGetCompTerminalId m_onGetCompTerminalId; // Richiede i nodeId dei terminali al circuito
		fnDeleteComponent m_onDeleteComponent;     // Richiede la rimozione di un componente al circuito
		fnOnSave m_onSave;
		fnOnLoad m_onLoad;
		fnOnNew m_onNew;

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

		void HandleEvents();

		void DrawImageGuiPanel();

		void DrawComponents();

		void DrawWires();

	public:
		UI(unsigned int width, unsigned int heigth, const std::string &title);
		~UI();

		// Setter per i callback - chiamati da Application nel costruttore
		void SetOnRunSimulation(const std::function<SimulationOutput()> &func) { m_onRunSimulation = func; }
		void SetOnCircuitChange(const fnCircuitChange &func) { m_onCircuitChange = func; }
		void SetOnCreateLink(const fnCreateLink &func) { m_onCreateLink = func; }
		void SetOnGetCompTerminalId(const fnGetCompTerminalId &func) { m_onGetCompTerminalId = func; }
		void SetOnDeleteComponent(const fnDeleteComponent &func) { m_onDeleteComponent = func; }
		void SetOnSave(const fnOnSave &func) { m_onSave = func; }
		void SetOnLoad(const fnOnLoad &func) { m_onLoad = func; }
		void SetOnNew(const fnOnNew &func) { m_onNew = func; }

		// Aggiunge la vista grafica di un componente al canvas
		void AddViewComponent(int compId, const std::string &name, ComponentType type, Vec2 position, float rotation);

		// Aggiunge la vista grafica di un filo al canvas
		void AddViewLink(int comp1, int term1, int comp2, int term2);

		// Rimuove tutte le viste grafiche (componenti e fili) dal canvas
		void Clear();

		// Restituisce la lista delle viste grafiche dei componenti (usata da IOManager per la serializzazione)
		const std::vector<ComponentView> &GetComponentsViewList() const { return m_componentViewList; }

		// Restituisce la lista delle viste grafiche dei fili (usata da IOManager per la serializzazione)
		const std::vector<LinkView> &GetLinkVIewList() const { return m_linkViewList; }

		// Avvia il loop principale: gestione eventi, aggiornamento ImGui, rendering
		void Run();
	};
}