#include "App/Application.h"

// Entry point: crea l'Application (che inizializza UI, Circuit, Solver e IOManager
// e collega tutti i callback tra loro) e avvia il loop principale.
int main()
{
    CircuitLab::Application app;
    app.Run();
    return 0;
}