#pragma once
#include <string>
#include <vector>
#include <map>
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
		std::string m_name;    // Nome visualizzato (es. "Resistor", "Voltage source")
		int m_componentLink;   // ID del Component corrispondente nel circuito
		ComponentType m_type;  // Tipo del componente (resistor, voltageSource, ground...)

		// Mappa statica: associa ogni ComponentType al suo ComponentDesign.
		// Definita in ComponentView.cpp, condivisa tra tutte le istanze.
		static const std::map<ComponentType, ComponentDesign> s_design;

	public:
		ComponentView(int componentLink, const Vec2 &position, float rotation,
			const std::string &name, ComponentType type);

		const Vec2 &GetPosition() const { return m_position; }
		float GetRotation() const { return m_rotation; }
		const std::string &GetName() const { return m_name; }
		int GetComponentLink() const { return m_componentLink; }
		ComponentType GetComponentType() const { return m_type; }

		// Restituisce il design grafico del tipo di questo componente
		const ComponentDesign &GetComponetDesign() const { return s_design.at(m_type); }

		void SetPosition(const Vec2 &position) { m_position = position; }
		void SetRotation(float rotation) { m_rotation = rotation; }
		void SetName(const std::string &name) { m_name = name; }
		void SetComponentLink(int link) { m_componentLink = link; }
		void SetComponentType(ComponentType type) { m_type = type; }
	};
}