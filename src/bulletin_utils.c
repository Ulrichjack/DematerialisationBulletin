#include "bulletin.h"

void afficher_eleve(const Eleve* eleve) {
    if (!eleve) {
        printf("Élève non trouvé\n");
        return;
    }
    
    printf("\n=== BULLETIN DE %s %s ===\n", eleve->nom, eleve->prenom);
    printf("ID: %d\n", eleve->id);
    printf("Classe: %s\n", eleve->classe);
    printf("Date: %s\n", eleve->date_bulletin);
    printf("Nombre de matières: %d\n", eleve->nombre_matieres);
    printf("Moyenne générale: %.2f\n", eleve->moyenne_generale);
    
    if (eleve->nombre_matieres > 0) {
        printf("\n--- MATIÈRES ---\n");
        for (int i = 0; i < eleve->nombre_matieres; i++) {
            printf("%s: %.2f (coef: %.1f)\n", 
                   eleve->matieres[i].nom_matiere,
                   eleve->matieres[i].note,
                   eleve->matieres[i].coefficient);
            if (strlen(eleve->matieres[i].appreciation) > 0) {
                printf("  Appréciation: %s\n", eleve->matieres[i].appreciation);
            }
        }
    }
    printf("========================\n");
}

void calculer_moyenne(Eleve* eleve) {
    if (!eleve || eleve->nombre_matieres == 0) {
        eleve->moyenne_generale = 0.0;
        return;
    }
    
    float somme_notes = 0.0;
    float somme_coefficients = 0.0;
    
    for (int i = 0; i < eleve->nombre_matieres; i++) {
        somme_notes += eleve->matieres[i].note * eleve->matieres[i].coefficient;
        somme_coefficients += eleve->matieres[i].coefficient;
    }
    
    if (somme_coefficients > 0) {
        eleve->moyenne_generale = somme_notes / somme_coefficients;
    } else {
        eleve->moyenne_generale = 0.0;
    }
}

void nettoyer_chaine(char* str) {
    if (!str) return;
    
    char* debut = str;
    while (*debut == ' ' || *debut == '\t' || *debut == '\n') {
        debut++;
    }
    
    if (debut != str) {
        memmove(str, debut, strlen(debut) + 1);
    }
    
    int len = strlen(str);
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\t' || str[len-1] == '\n')) {
        str[len-1] = '\0';
        len--;
    }
}