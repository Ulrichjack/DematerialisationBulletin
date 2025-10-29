#include "gui.h"

int main(int argc, char *argv[]) {
    // Toute la logique de l'application est maintenant gérée par l'interface graphique.
    // Le main se contente de lancer la GUI.
    init_gui(argc, argv);
    
    return 0;
}