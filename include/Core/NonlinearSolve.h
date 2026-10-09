#pragma once
#include <Eigen/Dense>
#include <optional>

#include "Core/Circuit.h"
#include "Core/Solver.h"

namespace CircuitLab {

	// Risolve lo step corrente di un circuito con componenti non lineari
	// (diodo, transistor) con Newton-Raphson: ad ogni iterazione ristampa il modello
	// companion sopra la parte lineare già calcolata da Circuit::ComputeMatrix /
	// ComputeVector, rifattorizza e risolve. Il circuito deve avere già la matrice e
	// il vettore lineari aggiornati per lo step (ComputeMatrix + ComputeVector).
	//
	// warmStart è la soluzione dello step precedente (le tensioni cambiano poco da
	// uno step all'altro, quindi di solito bastano 2-4 iterazioni); se la sua
	// dimensione non coincide (circuito modificato) si riparte da zero.
	// Restituisce nullopt se una matrice è singolare; converged dice se si è
	// arrivati a convergenza (altrimenti il vettore restituito è solo l'ultimo
	// tentativo e non va usato come soluzione).
	//
	// Un solo schema non basta, per questo si tenta in sequenza (si passa al
	// successivo solo se il precedente non converge, quindi il caso comune costa
	// una sola passata veloce):
	//
	// 1. Newton puro dalla soluzione precedente.
	//
	// 2. Newton con smorzamento ADATTIVO dalla soluzione precedente. Un componente a
	//    più giunzioni accoppiate (il transistor: base-emettitore e base-collettore
	//    si influenzano a vicenda tramite gm/go, vedi Transistor::StampNonlinear)
	//    può restare in un ciclo limite — il collettore oscilla fra due valori
	//    vicini senza mai stabilizzarsi — nella stretta zona di transizione
	//    accensione/interdizione, dove pnjlim (LimitVoltage) non interviene perché
	//    nessuna delle due tensioni supera mai la sua soglia. Lo smorzamento
	//    parte a passo pieno e si riduce SOLO quando la direzione dello spostamento
	//    si inverte rispetto all'iterazione precedente (il segno
	//    dell'oscillazione), per poi rilassarsi di nuovo. Uno smorzamento fisso non
	//    basta: abbastanza forte da rompere l'oscillazione (<= 0.2) è anche troppo
	//    lento per i casi ben comportati. Da solo, però, peggiora i circuiti
	//    rigenerativi (trigger di Schmitt), dove rallenta la salita verso l'altro
	//    stato: per questo non è il primo tentativo.
	//
	// 3. Newton puro da ZERO. In un circuito con isteresi, nell'istante in cui lo
	//    stato precedente smette di essere una soluzione (la tensione di ingresso
	//    ha superato la soglia) la soluzione si trova sull'altro ramo, e da lì
	//    Newton vaga senza stabilizzarsi (verificato sul trigger di Schmitt): partire
	//    da zero porta spesso sul ramo giusto.
	//
	// 4. Newton puro da RANDOM_RESTARTS partenze casuali (riproducibili). Dove
	//    la soluzione precedente non esiste più, la nuova è raggiungibile da molte
	//    partenze ma spesso non da zero (verificato: nel trigger di Schmitt il 70%
	//    delle partenze casuali converge, zero e source stepping no).
	//
	// 5. Source stepping (SPICE): le sorgenti sono portate da 0 al loro valore in
	//    SOURCE_STEPS passi, ciascuno risolto partendo dal precedente. Parte da una
	//    soluzione nota (tutto a zero) e segue un cammino continuo fino a quella
	//    cercata.
	std::optional<Eigen::VectorXd> SolveNonlinearStep(Circuit &circuit, Solver &solver,
		const Eigen::VectorXd &warmStart, bool &converged);

	// Calcola il PUNTO DI LAVORO DC del circuito: lo stato di regime con tutte le grandezze
	// costanti (condensatori aperti, induttori in corto, sorgenti al valore continuo; vedi
	// Circuit::SetDcAnalysis). Con componenti non lineari usa SolveNonlinearStep, quindi
	// anche i suoi tentativi di riserva (partenze casuali, source stepping), da zero.
	// Se converge imposta lo stato dinamico di ogni componente a quello del punto di lavoro
	// (Circuit::ApplyDcState) e restituisce il vettore soluzione, che ha lo stesso layout delle
	// incognite del transitorio e si può usare come partenza del primo passo. Il circuito
	// viene lasciato in modalità transitoria (da ricalcolare). nullopt se la matrice è
	// singolare; converged dice se si è arrivati a convergenza.
	std::optional<Eigen::VectorXd> SolveOperatingPoint(Circuit &circuit, Solver &solver, bool &converged);

	namespace NonlinearSolve {
		inline constexpr int FAST_ITERATIONS = 50;      // tentativo 1
		inline constexpr int DAMPED_ITERATIONS = 100;   // tentativo 2 e passi del 5
		inline constexpr int COLD_ITERATIONS = 100;     // tentativi 3 e 4
		inline constexpr int RANDOM_RESTARTS = 24;      // tentativo 4
		inline constexpr int SOURCE_STEPS = 10;         // tentativo 5

		inline constexpr double DAMPING_MIN = 0.1;
		inline constexpr double DAMPING_SHRINK = 0.5;   // fattore di riduzione quando la direzione si inverte
		inline constexpr double DAMPING_GROW = 1.5;     // fattore di recupero verso il passo pieno
	}
}
