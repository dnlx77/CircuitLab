#pragma once
#include <cstddef>

namespace CircuitLab {

	// Un passo di integrazione selezionabile dall'utente: valore in secondi e nome mostrato.
	struct TimestepOption {
		double seconds;
		const char *name;
	};

	inline constexpr TimestepOption TIMESTEPS[] = {
		{ 1.0, "1 s" }, { 0.1, "100 ms" }, { 0.01, "10 ms" }, { 0.001, "1 ms" },
		{ 1e-4, "100 us" }, { 1e-5, "10 us" }, { 1e-6, "1 us" }, { 1e-7, "100 ns" },
		{ 1e-8, "10 ns" }, { 1e-9, "1 ns" }, { 1e-10, "100 ps" }, { 1e-11, "10 ps" }, { 1e-12, "1 ps" },
	};
	inline constexpr std::size_t TIMESTEP_COUNT = sizeof(TIMESTEPS) / sizeof(TIMESTEPS[0]);
	inline constexpr std::size_t DEFAULT_TIMESTEP_INDEX = 3; // 1 ms

	// Velocità con cui il tempo simulato avanza rispetto al tempo reale (vedi
	// Application::SimulationLoop): 1.0 = tempo reale (il ritmo di default, utile per leggere
	// l'oscilloscopio come uno strumento vero). Un valore <= 0 è un segnale di "velocità
	// massima": nessuna pausa tra un batch e l'altro, il tempo simulato avanza alla velocità
	// con cui la CPU riesce a calcolare, utile per raggiungere in fretta un transitorio lungo
	// (es. l'assestamento di un filtro passa-alto RC con costante di tempo di alcuni secondi).
	//
	// Le velocità sotto 1 sono il "rallentatore": con segnali di decine o migliaia di Hz, a 1x
	// un fotogramma (16 ms) copre più periodi e la corrente letta ad ogni fotogramma è di
	// fatto casuale (aliasing), quindi i pallini saltano senza senso; a 0.01x un periodo di
	// 40 Hz dura 2.5 s reali e il movimento si segue.
	struct SpeedOption {
		double multiplier;
		const char *name;
	};

	inline constexpr SpeedOption SPEEDS[] = {
		{ 0.001, "0.001x" }, { 0.01, "0.01x" }, { 0.1, "0.1x" }, { 1.0, "1x" }, { 2.0, "2x" },
		{ 5.0, "5x" }, { 10.0, "10x" }, { 20.0, "20x" }, { 50.0, "50x" }, { 100.0, "100x" },
		{ -1.0, "Massima" },
	};
	inline constexpr std::size_t SPEED_COUNT = sizeof(SPEEDS) / sizeof(SPEEDS[0]);
	inline constexpr std::size_t DEFAULT_SPEED_INDEX = 3; // 1x
}
