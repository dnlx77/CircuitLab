#pragma once
#include <optional>
#include <string>

namespace CircuitLab::FileDialog {

	// Finestre di scelta file del sistema operativo (Windows: quelle standard di
	// Esplora risorse), così non serve scrivere a mano il nome del file. Sono
	// modali rispetto alla finestra owner (il valore di sf::Window::getNativeHandle(),
	// o nullptr) e BLOCCANO finché l'utente non sceglie o annulla: vanno chiamate
	// fuori da un frame ImGui in corso (vedi UI::Render).
	//
	// I percorsi sono in UTF-8, come il resto dell'interfaccia. Non cambiano la
	// cartella di lavoro del programma (da cui dipendono i font e le risorse).
	// Restituiscono nullopt se l'utente annulla; initialDir può essere vuota.

	std::optional<std::string> OpenCircuit(void *ownerWindow, const std::string &initialDir);

	// suggestedName compare già nella casella del nome (può essere vuoto);
	// se l'utente non scrive un'estensione si aggiunge ".json"
	std::optional<std::string> SaveCircuit(void *ownerWindow, const std::string &initialDir, const std::string &suggestedName);
}
