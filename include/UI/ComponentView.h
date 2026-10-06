#pragma once
#include <string>
#include <vector>
#include <map>
#include <SFML/Graphics.hpp>
#include "Core/Vector2.h"
#include "Common/ComponentType.h"

namespace CircuitLab {

	// Contiene le informazioni geometriche di un tipo di componente:
	// dimensioni del rettangolo di rappresentazione, raggio dei terminali
	// e offset dei terminali rispetto al centro del componente.
	// È una struttura dati pura, condivisa tra tutti i componenti dello stesso tipo.
	struct ComponentDesign {
		int compWidth, compHeight;			// Dimensioni del rettangolo del componente (pixel)
		int terminalRadius;					// Raggio del cerchio che rappresenta il terminale (pixel)
		std::vector<Vec2i> terminalOffset;	// Offset di ogni terminale rispetto al centro del componente
		int isPositiveTerminal;				// Indice del terminale eventualmente polarizzato (-1) se nessun polarizzato
	};

	// Rappresenta la vista grafica di un componente nel canvas.
	// Separa i dati visivi (posizione, rotazione, nome) dai dati di simulazione
	// che vivono nella controparte Component nel circuito.
	// Il collegamento tra le due parti avviene tramite m_componentLink (= Component::m_id).
	class ComponentView {
	private:
		Vec2 m_position;       // Posizione del centro del componente nel canvas (pixel)
		float m_rotation;      // Rotazione in gradi
		// Specchiato rispetto all'asse verticale locale (x -> -x), applicato PRIMA della
		// rotazione: per un transistor sposta la base da sinistra a destra. Vale per il
		// simbolo e per le posizioni dei terminali (vedi UI::GetRotatedTerminalPos).
		bool m_mirrored = false;
		std::string m_name;    // Nome visualizzato (es. "Resistor", "Voltage source")
		int m_componentLink;   // ID del Component corrispondente nel circuito
		ComponentType m_type;  // Tipo del componente (resistor, voltageSource, ground...)

		// Mappa statica: associa ogni ComponentType al suo ComponentDesign.
		// Definita in ComponentView.cpp, condivisa tra tutte le istanze.
		static const std::map<ComponentType, ComponentDesign> s_design;

	public:
		ComponentView(int componentLink, const Vec2 &position, float rotation,
			const std::string &name, ComponentType type, bool mirrored = false);

		const Vec2 &GetPosition() const { return m_position; }
		float GetRotation() const { return m_rotation; }
		bool IsMirrored() const { return m_mirrored; }
		// Vero se lo specchio cambia qualcosa: serve almeno un terminale fuori dall'asse
		// verticale (resistore, condensatore... sono simmetrici e non hanno nulla da specchiare)
		bool IsMirrorable() const
		{
			for (const auto &offset : GetComponetDesign().terminalOffset)
				if (offset.x != 0)
					return true;
			return false;
		}
		const std::string &GetName() const { return m_name; }
		int GetComponentLink() const { return m_componentLink; }
		ComponentType GetComponentType() const { return m_type; }

		// Restituisce il design grafico del tipo di questo componente
		const ComponentDesign &GetComponetDesign() const { return s_design.at(m_type); }

		// Disegna il simbolo schematico del componente (resistenza, condensatore,
		// massa, generatore, induttore, interruttore) in coordinate locali (origine
		// al centro, come gli offset dei terminali sopra), trasformato secondo
		// posizione e rotazione correnti. color distingue selezionato/non selezionato;
		// waveForm sceglie l'icona interna del generatore di tensione (+/-, sinusoide,
		// onda quadra); switchClosed sceglie se disegnare la lama dell'interruttore
		// chiusa o aperta. Entrambi i parametri sono ignorati dai tipi a cui non si
		// applicano. target è sf::RenderTarget (non sf::RenderWindow) apposta: sia il
		// canvas sia un sf::RenderTexture (usato per le icone della palette, vedi
		// UI::BuildPaletteIcons) derivano da RenderTarget.
		void DrawSymbol(sf::RenderTarget &target, sf::Color color, WaveFormType waveForm, bool switchClosed) const;

		void SetPosition(const Vec2 &position) { m_position = position; }
		void SetRotation(float rotation) { m_rotation = rotation; }
		void SetMirrored(bool mirrored) { m_mirrored = mirrored; }
		void SetName(const std::string &name) { m_name = name; }
		void SetComponentLink(int link) { m_componentLink = link; }
		void SetComponentType(ComponentType type) { m_type = type; }

		void Save(nlohmann::json &j) const;
	};
}