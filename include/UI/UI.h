#pragma once
#include <SFML/Graphics.hpp>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>

#include "Common/ComponentType.h"
#include "Common/SimulationOutput.h"
#include "UI/ComponentView.h"
#include "Common/UICommon.h"
#include "Common/ComponentValue.h"
#include "Common/OscilloscopeChannel.h"
#include "UI/WireGraph.h"

namespace CircuitLab {

	// Classe principale dell'interfaccia grafica.
	// Gestisce la finestra SFML, il loop degli eventi, il rendering dei componenti
	// e dei collegamenti, e il pannello ImGui.
	// Comunica con Application tramite un ampio set di callback (pattern Observer via
	// std::function), raggruppabili per area: gestione circuito (aggiungi/rimuovi
	// componente, collega terminali), simulazione (avvio, stato, oscilloscopio),
	// persistenza (save/load/new) e query sui valori/tipi dei componenti.
	class UI {
	public:
		// Callback per aggiungere un componente: riceve tipo e valore, restituisce l'ID assegnato
		using fnCircuitChange = std::function<int(CircuitLab::ComponentType type)>;
		// Callback per collegare due terminali: restituisce true se il collegamento è andato a buon fine
		using fnCreateLink = std::function<bool(int compId1, int termIndex1, int compId2, int termIndex2)>;
		// Callback per ottenere i nodeId dei terminali di un componente
		using fnGetCompTerminalId = std::function<std::vector<int>(int compId)>;
		// Callback per eliminare un componente dal circuito
		using fnDeleteComponent = std::function<void(int compId)>;
		// Callback per lasciare libero un terminale (nodeId -1) quando la UI ne toglie il filo
		using fnFreeTerminal = std::function<void(int compId, int termIndex)>;
		// Callback per togliere dalla massa un gruppo di terminali che il disegno mostra ancora uniti
		using fnDetachFromGround = std::function<void(const std::vector<std::pair<int, int>> &terminals)>;

		using fnOnSave = std::function<void(const std::string &filePath)>; // Callback per salvare il circuito su file
		using fnOnLoad = std::function<void(const std::string &filePath)>; // Callback per caricare il circuito da file
		using fnOnNew = std::function<void()>;                             // Callback per resettare il canvas
		using fnGetComponentValues = std::function<std::map<ComponentValue, double>(int compId)>;
		using fnSetComponentValues = std::function<void(int compId, const std::map<ComponentValue, double> &values)>;
		using fnToggleSwitch = std::function<void(int compId)>;
		using fnIsSwitchClosed = std::function<bool(int compId)>;
		using fnSetSimulationStatus = std::function<void(const SimulationStatus status)>;
		using fnGetSimulationStatus = std::function<SimulationStatus()>;
		using fnGetComponentTypeById = std::function<ComponentType(int compId)>;
		using fnGetWaveFormType = std::function<WaveFormType(int)>;
		using fnSetWaveFormType = std::function<void(int, WaveFormType)>;
		using fnGetOscilloscopeChannels = std::function<std::vector<OscilloscopeChannel>()>;
		using fnAddChannel = std::function<void(ProbeType, int, int, int)>;
		using fnSetChannelActive = std::function<void(int, bool)>;
		using fnRemoveChannel = std::function<void(int)>;
		using fnGetHSim = std::function<double()>;
		using fnSetHSim = std::function<void(int)>;
		using fnSetSimSpeed = std::function<void(int)>;
		using fnSetStartFromDc = std::function<void(bool)>;
		using fnRestartFromDc = std::function<void()>;
		using fnSetWindowTime = std::function<void(double)>;
		using fnAutoSync = std::function<void()>;
		using fnGetSimulationTime = std::function<double()>;
		using fnGetDecimationFactor = std::function<int()>;
		using fnGetMaxFrequency = std::function<double()>;

		// Undo/redo (vedi Application::PushUndoSnapshot/Undo/Redo): la UI chiama
		// fnPushUndoSnapshot PRIMA di ogni gesto che muta lo stato (piazzare,
		// cancellare, spostare, ruotare, collegare, cambiare un valore...), mai
		// dopo. fnCanUndo/fnCanRedo servono solo a disabilitare i pulsanti nel
		// pannello quando non c'è nulla da annullare/ripetere.
		using fnPushUndoSnapshot = std::function<void()>;
		using fnUndo = std::function<void()>;
		using fnRedo = std::function<void()>;
		using fnCanUndo = std::function<bool()>;
		using fnCanRedo = std::function<bool()>;
		
	private:
		unsigned int m_width;   // Larghezza della finestra (pixel)
		unsigned int m_height;  // Altezza della finestra (pixel)
		std::string m_title;    // Titolo della finestra
		sf::Font m_font;        // Font usato per le etichette dei componenti sul canvas
		sf::View m_view;


		// Costanti di configurazione UI
		static constexpr int CLICK_TOLERANCE = 7;         // Tolleranza click sui terminali (pixel)
		// Distanza minima (pixel di schermo) perché il trascinamento di un NodeView
		// sia considerato tale, invece che il tremolio inevitabile di un semplice
		// click. Senza questa soglia, un right-click che selezionava un pallino
		// ANCORATO (i pallini si prendono prima del corpo, vedi CheckClick) lo
		// staccava già al primo, minimo movimento del mouse — perdendo l'aggancio
		// col terminale senza che l'utente se ne accorgesse (bug osservato: un filo
		// rimasto indietro dopo aver spostato di nuovo il componente).
		static constexpr float NODE_DRAG_THRESHOLD = 4.0f;
		static constexpr float DEFAULT_ROTATION = 0.0f;    // Rotazione di default (gradi)
		static constexpr int OUTLINE_THICKNESS = 2;        // Spessore outline selezione (pixel)
		static constexpr int TEXT_COMPONENT_OFFSET = 15;   // Offset orizzontale etichetta rispetto al centro del componente (pixel)
		// Layout della finestra: barra dei menu in alto (altezza dettata da ImGui, vedi
		// m_menuBarHeight), sotto la barra degli strumenti, in fondo la barra di stato; il canvas
		// occupa tutto lo spazio in mezzo, a tutta larghezza.
		static constexpr float TOOLBAR_HEIGHT = 36.0f;
		static constexpr float STATUS_BAR_HEIGHT = 24.0f;
		float m_menuBarHeight = 19.0f;
		static constexpr int NODE_RADIUS = 4;
		static constexpr float EPSILON = 1.5f;
		static constexpr int PARTICLE_SIZE = 5;
		static constexpr int PARTICLE_SPACING_FACTOR = 3;
		static constexpr float PARTICLE_SPEED_SCALE = 0.1f;
		// Impostazioni dei pallini della corrente, dal menu Visualizza: visibilità e
		// moltiplicatore di velocità (1 = PARTICLE_SPEED_SCALE, valori più bassi rallentano)
		bool m_showParticles = true;
		float m_particleSpeed = 1.0f;
		// Sotto questa soglia (in Ampere) la corrente è considerata nulla ai fini
		// del colore dei pallini, per evitare sfarfallii tra i due colori per
		// rumore numerico attorno allo zero.
		static constexpr double PARTICLE_CURRENT_SIGN_EPSILON = 1e-9;
		inline static const sf::Color PARTICLE_COLOR_POSITIVE = sf::Color(255, 140, 0);  // Arancio: corrente nel verso positivo del filo
		inline static const sf::Color PARTICLE_COLOR_NEGATIVE = sf::Color(0, 200, 255);  // Ciano: corrente nel verso opposto
		inline static const sf::Color PARTICLE_COLOR_NEUTRAL = sf::Color::Yellow;        // Corrente ~0
		inline static const sf::Color BACKGROUND_COLOR = sf::Color(30, 30, 30); // Colore sfondo canvas

		// Zoom del canvas (rotellina del mouse). m_view è la vista del canvas:
		// il suo size vale (dimensione canvas / m_zoom), quindi zoom > 1 ingrandisce.
		static constexpr float ZOOM_MIN = 0.25f;
		static constexpr float ZOOM_MAX = 4.0f;
		static constexpr float ZOOM_STEP = 1.1f;  // Fattore moltiplicativo per ogni "tacca" di rotellina
		float m_zoom = 1.0f;

		// Pan del canvas: trascinando col tasto centrale, m_panLastPixel è l'ultima
		// posizione del cursore (in pixel di finestra) da cui calcolare lo spostamento.
		bool m_panning = false;
		sf::Vector2i m_panLastPixel;

		// Griglia di allineamento: visibilità e aggancio (snap) sono indipendenti.
		// Passo espresso in unità mondo (non dipende dallo zoom).
		static constexpr float GRID_MIN_SCREEN_SPACING = 8.0f;  // Sotto questa distanza a schermo il passo di disegno raddoppia
		static constexpr int GRID_MAJOR_EVERY = 5;              // Una linea ogni N più marcata
		bool m_showGrid = true;
		bool m_snapToGrid = true;
		int m_gridSize = 20;

		SimulationOutput m_simulationOutput;  // Ultimo risultato di simulazione ricevuto
		bool m_showOscilloscope;
		bool m_showResults = true;            // finestra "Risultati" (tensioni e correnti)

		// Tipo di componente scelto dal menu Componenti: finché è impostato il simbolo segue il
		// mouse sul canvas e ogni click sinistro ne posa uno (Esc o click destro per finire)
		std::optional<ComponentType> m_placingType;
		double m_windowTime;

		int m_hSimIndex;

		// Altezza della finestra dei dati del componente nel frame precedente: serve a
		// decidere se c'è spazio per aprirla sopra al componente (vedi DrawComponentPopup)
		float m_compPopupHeight = 200.0f;
		float m_compPopupWidth = 270.0f;

		// File del circuito corrente (dopo Apri / Salva con nome): "Salva" scrive qui senza
		// chiedere ogni volta il percorso. Vuoto per un circuito nuovo.
		std::string m_currentFilePath;
		// Azione richiesta da un pulsante o da una scorciatoia: la finestra di scelta file
		// è bloccante, quindi si esegue a fine frame (vedi ProcessPendingFileAction) e non
		// nel mezzo dei widget ImGui.
		enum class FileAction { none, open, save, saveAs };
		FileAction m_pendingFileAction = FileAction::none;
		int m_simSpeedIndex;
		bool m_startFromDc = false; // vedi Application::m_startFromDc (stesso valore iniziale)

		// Misura della velocità effettiva (tempo simulato / tempo reale) mostrata nel pannello
		double m_speedSampleWall = 0.0;
		double m_speedSampleSim = 0.0;
		double m_measuredSpeed = 0.0;

		int m_oscProbeType = 0;    // indice nel combo ProbeType
		int m_oscIdA = 0;
		int m_oscIdB = 0;
		int m_oscCompId = 0;

		// Congela la finestra temporale dell'oscilloscopio: mentre è true, l'asse X
		// smette di scorrere (niente più ImPlotCond_Always) e mantiene l'ultimo
		// tMax noto, così l'utente può zoomare/scorrere liberamente sull'asse X
		// esattamente come già può fare sull'asse Y (che usa ImPlotCond_Once).
		// Non basta congelare l'asse: la simulazione in background continua a
		// riempire/svuotare il deque dei campioni, quindi va congelato anche
		// il DATO plottato, non solo la sua posizione sull'asse X.
		// m_frozenChannels viene aggiornato ad ogni frame in cui NON si è congelati
		// (stesso trucco di m_frozenTMax): appena si attiva Freeze, resta fermo
		// sull'ultimo snapshot live.
		bool m_oscFrozen = false;

		// Modalità di visualizzazione dell'oscilloscopio:
		//  - scorrimento: l'asse X avanza col tempo e la traccia "cammina" (come un
		//    registratore a striscia);
		//  - sweep: l'asse X è una finestra fissa [0, finestra] e la traccia parte
		//    ogni volta da sinistra ridisegnandosi sopra la precedente, come uno
		//    scope reale. L'inizio di ogni passata è allineato al periodo della
		//    frequenza più alta del circuito (come Auto Sync), quindi un segnale
		//    periodico si ripresenta sempre nella stessa posizione.
		static constexpr int OSC_MODE_ROLLING = 0;
		static constexpr int OSC_MODE_SWEEP = 1;
		// X-Y: il primo canale attivo è l'asse X e il secondo l'asse Y (niente asse dei
		// tempi), per vedere cose come l'isteresi di un trigger di Schmitt (ingresso
		// sull'asse X, uscita sull'asse Y) o le figure di Lissajous.
		static constexpr int OSC_MODE_XY = 2;
		int m_oscMode = OSC_MODE_ROLLING;

		// Formato dei numeri sugli assi dell'oscilloscopio (scelto dall'utente)
		static constexpr int OSC_NOTATION_AUTO = 0;        // formato predefinito di ImPlot
		static constexpr int OSC_NOTATION_SCIENTIFIC = 1;  // 2.5e-3
		static constexpr int OSC_NOTATION_SI = 2;          // 2.5 ms, 2.5 mV
		int m_oscNotation = OSC_NOTATION_AUTO;

		// Adatta continuamente l'asse Y ai dati visibili (di default l'asse partiva
		// fisso a +-15 e andava regolato a mano)
		bool m_oscAutoY = true;

		// Mostra sotto il grafico la tabella delle misure (Vpp, media, RMS, frequenza)
		// calcolate sulla porzione di campioni oggi visibile
		bool m_oscShowMeasures = false;
		double m_frozenTMax = 0.0;
		std::vector<OscilloscopeChannel> m_frozenChannels;

		SelectedComponent m_selectedComponent; // Componente/terminale attualmente selezionato

		std::vector<ComponentView> m_componentViewList; // Lista delle viste grafiche dei componenti
		// Topologia dei collegamenti (NodeView e fili): vive in WireGraph, che ne
		// contiene tutte le operazioni di modifica e si prova da sola. Le liste e i
		// contatori qui sotto ne sono riferimenti, per leggerli e disegnarli direttamente.
		WireGraph m_graph;
		std::vector<LinkView> &m_linkViewList = m_graph.linkViews;   // Lista dei collegamenti (fili) da disegnare
		std::vector<NodeView> &m_nodeViewList = m_graph.nodeViews;
		unsigned int &m_linkViewIdCount = m_graph.linkViewIdCount;
		unsigned int &m_nodeViewCount = m_graph.nodeViewCount;
		std::unordered_map<int, double> m_linkViewCurrentList;
		std::vector<LinkParticles> m_linkParticlesList;

		sf::Vector2f m_compClickOffset;
		void SetDragOffsetFromComponent(int compId, sf::Vector2i pos);
		void SetDragOffsetFromNodeView(int nodeViewId, sf::Vector2i pos);

		// Selezione multipla (rettangolo di gomma, tasto sinistro trascinato da
		// spazio vuoto): gli ID dei componenti e dei NodeView LIBERI (non i
		// pallini ancorati a un terminale, che seguono già il loro componente da
		// soli) attualmente selezionati in blocco, trascinabili insieme
		// (SelectionState::draggingSelection, vedi HandleEvents) e cancellabili
		// insieme (tasto Delete). Indipendente da m_selectedComponent, che resta
		// lo stato della selezione SINGOLA.
		std::vector<int> m_selectedComponentIds;
		std::vector<int> m_selectedNodeViewIds;
		bool m_selectingRect = false;             // true mentre si sta trascinando il rettangolo
		sf::Vector2f m_selectionRectStart;         // angolo del rettangolo dove è iniziato il trascinamento (coordinate mondo)
		sf::Vector2f m_selectionRectCurrent;       // angolo opposto, aggiornato ad ogni MouseMoved (coordinate mondo)

		// Icone pre-renderizzate (una per tipo di componente della palette, mostrate nel menu
		// Componenti) su una piccola RenderTexture: il simbolo è statico
		// (nessuna posa dipende dallo stato di un'istanza reale), quindi si
		// disegna una sola volta, nel costruttore, invece che ogni frame.
		std::map<ComponentType, sf::RenderTexture> m_paletteIcons;

		sf::RenderWindow m_window; // Finestra SFML
		sf::Clock m_deltaClock;

		// Callback impostati da Application
		fnCircuitChange m_onCircuitChange;
		fnCreateLink m_onCreateLink;
		fnGetCompTerminalId m_onGetCompTerminalId; // Richiede i nodeId dei terminali al circuito
		fnDeleteComponent m_onDeleteComponent;     // Richiede la rimozione di un componente al circuito
		fnFreeTerminal m_onFreeTerminal;           // Richiede al circuito di liberare un terminale il cui filo è stato tolto
		fnDetachFromGround m_onDetachFromGround;   // Richiede al circuito di staccare dalla massa un gruppo di terminali
		fnOnSave m_onSave;
		fnOnLoad m_onLoad;
		fnOnNew m_onNew;
		fnGetComponentValues m_onGetComponentValues;
		fnSetComponentValues m_onSetComponentValues;
		fnToggleSwitch m_onToggleSwitch;
		fnIsSwitchClosed m_onIsSwitchClosed;
		fnSetSimulationStatus m_onSetSimulationStatus;
		fnGetSimulationStatus m_onGetSimulationStatus;
		fnGetComponentTypeById m_onGetComponentTypeById;
		fnGetWaveFormType m_onGetWaveFormType;
		fnSetWaveFormType m_onSetWaveFormType;
		fnGetOscilloscopeChannels m_onGetOscilloscopeChannels;
		fnAddChannel m_onAddChannel;
		fnSetChannelActive m_onSetChannelActive;
		fnRemoveChannel m_onRemoveChannel;
		fnGetHSim m_onGetHSim;
		fnSetHSim m_onSetHSim;
		fnSetSimSpeed m_onSetSimSpeed;
		fnSetStartFromDc m_onSetStartFromDc;
		fnRestartFromDc m_onRestartFromDc;
		fnSetWindowTime m_onSetWindowTime;
		fnAutoSync m_onAutoSync;
		fnGetSimulationTime m_onGetSimulationTime;
		fnGetDecimationFactor m_onGetDecimationFactor;
		fnGetMaxFrequency m_onGetMaxFrequency;
		fnPushUndoSnapshot m_onPushUndoSnapshot;
		// Cattura lo stato PRIMA di un gesto che lo modifica (vedi Application::PushUndoSnapshot)
		void PushUndo() { if (m_onPushUndoSnapshot) m_onPushUndoSnapshot(); }
		fnUndo m_onUndo;
		fnRedo m_onRedo;
		fnCanUndo m_onCanUndo;
		fnCanRedo m_onCanRedo;

		// Determina quale componente o terminale è stato cliccato nella posizione pos.
		// Aggiorna selComp con il risultato.
		void CheckClick(sf::Vector2i pos, SelectedComponent &selComp);

		// Calcola le coordinate pixel dei due estremi di un collegamento,
		// tenendo conto delle posizioni e degli offset dei terminali.
		CircuitLab::LinkView GetLinkCoords(int comp1, int term1, NodeView nodeView);

		// Calcola la posizione ruotata di un terminale nel canvas,
		// tenendo conto della rotazione del componente.
		sf::Vector2f GetRotatedTerminalPos(const ComponentView &cw, int termIndex) const;

		// Aggiorna le coordinate dei fili collegati a un componente
		// dopo che quest'ultimo è stato ruotato o spostato.
		void UpdateLinksForComponent(int compId);

		std::vector<sf::Vector2f> GetTerminalPositionbyCompId(int compId) const;

		// Restituisce il NodeView con l'ID dato (lancia eccezione se non trovato)
		const NodeView &GetNodeViewById(int nodeViewId) const;

		// Converte una posizione in pixel della finestra in coordinate mondo
		// (tenendo conto di zoom e spostamento della vista), arrotondata all'intero.
		sf::Vector2i WorldPos(sf::Vector2i pixelPos) const;

		// Riporta lo zoom a 100% e la vista alla posizione iniziale
		void ResetZoom();

		// Geometria del canvas, in pixel della finestra (vedi TOOLBAR_HEIGHT)
		float CanvasTop() const { return m_menuBarHeight + TOOLBAR_HEIGHT; }
		float CanvasWidth() const { return static_cast<float>(m_width); }
		float CanvasHeight() const { return static_cast<float>(m_height) - CanvasTop() - STATUS_BAR_HEIGHT; }
		// Vero se il punto (pixel della finestra) cade sul canvas e non sulle barre
		bool IsInCanvas(sf::Vector2i pixelPos) const;
		// Porta dimensione (secondo lo zoom) e viewport della vista del canvas alla geometria corrente
		void UpdateCanvasView();

		// Aggiorna dimensioni finestra, vista del canvas e viewport dopo che
		// l'utente ha ridimensionato la finestra trascinandone il bordo.
		void HandleResize(unsigned int newWidth, unsigned int newHeight);

		// Se l'aggancio è attivo restituisce il nodo di griglia più vicino a p, altrimenti p
		sf::Vector2f SnapToGrid(sf::Vector2f p) const;

		// Sposta il componente in modo che il suo primo terminale cada su un nodo
		// di griglia (no-op se l'aggancio è disattivato)
		void SnapComponentToGrid(ComponentView &cw);

		// Disegna le linee della griglia sull'area di mondo visibile
		void DrawGrid();

		// Disegna il rettangolo di selezione multipla mentre lo si sta trascinando
		// (vedi m_selectingRect), in coordinate mondo come il resto del canvas.
		void DrawSelectionRect();

		// Rimuove il componente id dal canvas e dal circuito: vista, fili/NodeView
		// collegati (con la cascata di pulizia già gestita da WireGraph) e il
		// componente stesso nel Circuit. Estratto dalla gestione del tasto Delete
		// per poterlo applicare una volta per ogni componente di un gruppo
		// selezionato, non solo alla selezione singola.
		void DeleteComponent(int id);

		// Interfaccia ImGui: barra dei menu (File, Modifica, Componenti, Simulazione, Visualizza),
		// barra degli strumenti con i comandi di simulazione, barra di stato in fondo e finestra dei
		// risultati. DrawMainMenuBar va chiamata per prima (aggiorna m_menuBarHeight).
		void DrawMainMenuBar();
		void DrawToolbar();
		void DrawStatusBar();
		void DrawResultsWindow();
		void DrawComponentPopup();

		// Azioni condivise da menu e scorciatoie
		void NewCircuit();
		void RotateSelectedComponent();
		void DeleteSelection();
		// Disegna in trasparenza, sotto il mouse, il componente che si sta per posare (m_placingType)
		void DrawPlacementGhost();
		void ProcessPendingFileAction();
		void MirrorComponent(int compId);

		// Crea un componente del tipo dato in posizione pos (coordinate mondo) e lo
		// aggiunge al canvas con lo snap alla griglia già usato per i tasti R/V/G/C/
		// L/S/D/T/Y: unico punto in cui passano sia il piazzamento da tastiera sia
		// quello dal menu Componenti (modalità di piazzamento), vedi DrawMainMenuBar.
		void PlaceNewComponent(ComponentType type, sf::Vector2i pos);

		// Popola m_paletteIcons: per ogni tipo della palette disegna il simbolo
		// IEC (ComponentView::DrawSymbol) su una RenderTexture dedicata. Chiamato
		// una sola volta dal costruttore.
		void BuildPaletteIcons();

		// Disegna tutti i componenti (corpo + terminali + etichette) nel canvas
		void DrawComponents();

		// Disegna tutti i fili (LinkView) tra terminali e NodeView
		void DrawWires();

		// Disegna i NodeView visibili (giunzioni con più di 2 link)
		void DrawNodes();

		// Disegna le particelle di corrente (stile Falstad) lungo il filo linkId
		void DrawParticles(const std::unordered_map<int, sf::Vector2f> &nodePositions);

		// Disegna la finestra ImPlot dell'oscilloscopio con i canali attivi
		void DrawOscilloscope();

		// Converte un ComponentValue (enum) nella label testuale mostrata nella UI
		std::string_view ComponentValueToString(ComponentValue value);

		// Distanza perpendicolare del punto P dal segmento (o retta) A-B, usata per il click sui fili
		float PointToStraightDistance(const sf::Vector2f &A, const sf::Vector2f &B, const sf::Vector2f &P);

		// Avanza la posizione (offset) di ogni particella attiva di dt secondi,
		// facendo ripartire le particelle che escono dal filo
		void UpdateParticles(float dt);

		// Posizione nel canvas del NodeView con l'ID dato
		sf::Vector2f GetNodeviewPositionByNodeViewId(int nodeViewId);

		// Aggiorna la posizione del NodeView e delle LinkView che vi convergono dopo un trascinamento
		void UpdateLinksForNodeView(int nodeViewId, sf::Vector2f newPos);

	public:
		UI(unsigned int width, unsigned int height, const std::string &title);
		~UI();

		// Setter per i callback - chiamati da Application nel costruttore
		void SetOnCircuitChange(const fnCircuitChange &func) { m_onCircuitChange = func; }
		void SetOnCreateLink(const fnCreateLink &func) { m_onCreateLink = func; }
		void SetOnGetCompTerminalId(const fnGetCompTerminalId &func) { m_onGetCompTerminalId = func; }
		void SetOnDeleteComponent(const fnDeleteComponent &func) { m_onDeleteComponent = func; }
		void SetOnFreeTerminal(const fnFreeTerminal &func) { m_onFreeTerminal = func; }
		void SetOnDetachFromGround(const fnDetachFromGround &func) { m_onDetachFromGround = func; }
		void SetOnSave(const fnOnSave &func) { m_onSave = func; }
		void SetOnLoad(const fnOnLoad &func) { m_onLoad = func; }
		void SetOnNew(const fnOnNew &func) { m_onNew = func; }
		void SetOnGetComponentValues(const fnGetComponentValues &func) { m_onGetComponentValues = func; }
		void SetOnSetComponentValues(const fnSetComponentValues &func) { m_onSetComponentValues = func; }
		void SetOnToggleSwitch(const fnToggleSwitch &func) { m_onToggleSwitch = func; }
		void SetOnIsSwitchClosed(const fnIsSwitchClosed &func) { m_onIsSwitchClosed = func; }
		void SetOnSetSimulationStatus(const fnSetSimulationStatus &func) { m_onSetSimulationStatus = func; }
		void SetOnGetSimulationStatus(const fnGetSimulationStatus &func) { m_onGetSimulationStatus = func; }
		void SetOnGetComponentTypeById(const fnGetComponentTypeById &func) { m_onGetComponentTypeById = func; }
		void SetOnGetWaveFormType(const fnGetWaveFormType &func) { m_onGetWaveFormType = func; }
		void SetOnSetWaveFormType(const fnSetWaveFormType &func) { m_onSetWaveFormType = func; }
		void SetOnGetOscilloscopeChannels(const fnGetOscilloscopeChannels &func) { m_onGetOscilloscopeChannels = func; }
		void SetOnAddChannel(const fnAddChannel &fn) { m_onAddChannel = fn; }
		void SetOnSetChannelActive(const fnSetChannelActive &func) { m_onSetChannelActive = func; }
		void SetOnRemoveChannel(const fnRemoveChannel &func) { m_onRemoveChannel = func; }
		void SetOnGetHSim(const fnGetHSim &func) { m_onGetHSim = func; }
		void SetOnSetHSim(const fnSetHSim &func) { m_onSetHSim = func; }
		void SetOnSetSimSpeed(const fnSetSimSpeed &func) { m_onSetSimSpeed = func; }
		void SetOnSetStartFromDc(const fnSetStartFromDc &func) { m_onSetStartFromDc = func; }
		void SetOnRestartFromDc(const fnRestartFromDc &func) { m_onRestartFromDc = func; }
		void SetOnSetWindowTime(const fnSetWindowTime &func) { m_onSetWindowTime = func; }
		void SetOnAutoSync(const fnAutoSync &func) { m_onAutoSync = func; }
		void SetOnGetSimulationTime(const fnGetSimulationTime &func) { m_onGetSimulationTime = func; }
		void SetOnGetDecimationFactor(const fnGetDecimationFactor &func) { m_onGetDecimationFactor = func; }
		void SetOnGetMaxFrequency(const fnGetMaxFrequency &func) { m_onGetMaxFrequency = func; }
		void SetOnPushUndoSnapshot(const fnPushUndoSnapshot &func) { m_onPushUndoSnapshot = func; }
		void SetOnUndo(const fnUndo &func) { m_onUndo = func; }
		void SetOnRedo(const fnRedo &func) { m_onRedo = func; }
		void SetOnCanUndo(const fnCanUndo &func) { m_onCanUndo = func; }
		void SetOnCanRedo(const fnCanRedo &func) { m_onCanRedo = func; }

		// Aggiunge la vista grafica di un componente al canvas
		void AddViewComponent(int compId, const std::string &name, ComponentType type, Vec2 position, float rotation, bool mirrored = false);

		// Aggiunge la vista grafica di un filo al canvas
		int AddViewLink(int comp1, int term1, int nodeViewId);

		// Aggiunge un tratto di bus (filo tra due NodeView, nessun componente
		// coinvolto) al canvas; restituisce l'ID assegnato alla nuova LinkView.
		// Usato dal caricamento da file (vedi IOManager); a runtime i tratti di bus si
		// creano tramite WireGraph (m_graph), che li registra anche sui due NodeView.
		int AddBusLinkView(int sourceNodeViewId, int targetNodeViewId);

		// Collega due elementi selezionati con un tratto di bus (un "filo"): terminale,
		// nodo libero, o un punto sul mezzo di un filo esistente (dove nasce un nodo
		// libero, per una derivazione a T). Il Circuit viene avvisato PRIMA del disegno
		// e, se rifiuta il collegamento, non si disegna nulla. Non fa nulla se i due
		// elementi sono già nello stesso nodo (si formerebbe un ciclo di fili).
		void ConnectSelections(const SelectedComponent &first, const SelectedComponent &second);

		// Comunica al Circuit i terminali a cui la UI ha tolto ogni filo (vedi
		// WireGraph::RemoveComponent): tornano liberi anche per lui.
		void FreeTerminals(const std::vector<TerminalRef> &terminals);

		// Dopo aver cancellato un Ground: i gruppi di terminali che il disegno mostra
		// ancora uniti ma che non hanno più nessuna massa vengono staccati dallo 0 nel
		// Circuit (restano collegati tra loro). candidates = i terminali che erano nel
		// gruppo del Ground, raccolti prima della cancellazione.
		void DetachSurvivingGroupsFromGround(const std::vector<TerminalRef> &candidates);

		// Aggiunge un NodeView al canvas; restituisce l'ID assegnato. Con anchorCompId
		// != -1 è ancorato a quel terminale (vedi NodeView).
		int AddNodeView(int nodeId, sf::Vector2f position, int anchorCompId = -1, int anchorTermIndex = -1, bool attached = false);

		// Sostituisce la lista dei linkViewIds appartenenti al NodeView nodeViewId (usato da IOManager al caricamento)
		void UpdateNodeViewLinkIds(int nodeViewId, std::vector<int> linkViewIds);

		// Rimuove tutte le viste grafiche (componenti e fili) dal canvas
		void Clear();

		// Processa tutti gli eventi SFML/ImGui del frame corrente (click, drag, tasti, chiusura finestra)
		void HandleEvents();

		// Disegna un frame completo: canvas, componenti, fili, particelle, pannello ImGui
		void Render();

		// Restituisce la lista delle viste grafiche dei componenti (usata da IOManager per la serializzazione)
		const std::vector<ComponentView> &GetComponentsViewList() const { return m_componentViewList; }

		// Restituisce la lista delle viste grafiche dei fili (usata da IOManager per la serializzazione)
		const std::vector<LinkView> &GetLinkViewList() const { return m_linkViewList; }

		const std::vector<NodeView> &GetNodeViewList() const { return m_nodeViewList; }

		bool IsWindowOpen() const { return m_window.isOpen(); }

		void UpdateSimulation(SimulationOutput output) { m_simulationOutput = std::move(output); }

		// Ricostruisce m_linkParticlesList in base al numero di particelle da mostrare per ogni filo
		void CreateLinkParticlesList();

		// Ricostruisce m_linkViewCurrentList leggendo le correnti di ramo dall'ultimo SimulationOutput
		void CreateLinkViewCurrentList();

		void SetWindowTime(double windowTIme) { m_windowTime = windowTIme; }
	};
}