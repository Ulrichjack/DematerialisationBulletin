#include "bulletin.h"

void afficher_eleve(const Eleve* eleve) {
    if (!eleve) {
        printf("Élève non trouvé\n");
        return;
    }
    
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                     BULLETIN SCOLAIRE                          ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");
    
    // Informations personnelles
    printf("📋 INFORMATIONS PERSONNELLES\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    printf("  ID Système      : %d\n", eleve->id);
    printf("  Nom complet     : %s %s\n", eleve->nom, eleve->prenom);
    
    if (strlen(eleve->matricule) > 0 && strcmp(eleve->matricule, "N/A") != 0) {
        printf("  Matricule       : %s\n", eleve->matricule);
    }
    
    if (strlen(eleve->date_naissance) > 0 && strcmp(eleve->date_naissance, "N/A") != 0) {
        printf("  Date naissance  : %s\n", eleve->date_naissance);
    }
    
    if (strlen(eleve->lieu_naissance) > 0 && strcmp(eleve->lieu_naissance, "N/A") != 0) {
        printf("  Lieu naissance  : %s\n", eleve->lieu_naissance);
    }
    
    printf("\n🏫 INFORMATIONS ACADÉMIQUES\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    printf("  Classe          : %s\n", eleve->classe);
    
    if (strlen(eleve->niveau) > 0 && strcmp(eleve->niveau, "N/A") != 0) {
        printf("  Niveau          : %s\n", eleve->niveau);
    }
    
    if (strlen(eleve->annee_academique) > 0 && strcmp(eleve->annee_academique, "N/A") != 0) {
        printf("  Année           : %s\n", eleve->annee_academique);
    }
    
    printf("  Date bulletin   : %s\n", eleve->date_bulletin);
    printf("  Nombre matières : %d\n", eleve->nombre_matieres);
    printf("  Moyenne générale: %.2f/20\n", eleve->moyenne_generale);
    
    // Affichage des matières
    if (eleve->nombre_matieres > 0) {
        printf("\n📚 NOTES PAR MATIÈRE\n");
        printf("─────────────────────────────────────────────────────────────────\n");
        printf("%-45s %8s %8s\n", "Matière", "Note", "Coef");
        printf("─────────────────────────────────────────────────────────────────\n");
        
        for (int i = 0; i < eleve->nombre_matieres; i++) {
            // Couleur selon la note (si terminal supporte ANSI)
            char *couleur = "";
            if (eleve->matieres[i].note >= 16.0) couleur = "\033[1;32m"; // Vert
            else if (eleve->matieres[i].note >= 14.0) couleur = "\033[1;36m"; // Cyan
            else if (eleve->matieres[i].note >= 10.0) couleur = "\033[1;33m"; // Jaune
            else couleur = "\033[1;31m"; // Rouge
            
            printf("%-45s %s%7.2f\033[0m %7.1f\n", 
                   eleve->matieres[i].nom_matiere,
                   couleur,
                   eleve->matieres[i].note,
                   eleve->matieres[i].coefficient);
            
            if (strlen(eleve->matieres[i].appreciation) > 0) {
                printf("  └─ Appréciation: %s\n", eleve->matieres[i].appreciation);
            }
        }
        printf("─────────────────────────────────────────────────────────────────\n");
    }
    
    // Appréciation générale
    printf("\n🎯 RÉSULTAT GLOBAL\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    printf("  Moyenne générale: ");
    
    if (eleve->moyenne_generale >= 16.0) {
        printf("\033[1;32m%.2f/20 - Très bien ✨\033[0m\n", eleve->moyenne_generale);
    } else if (eleve->moyenne_generale >= 14.0) {
        printf("\033[1;36m%.2f/20 - Bien 👍\033[0m\n", eleve->moyenne_generale);
    } else if (eleve->moyenne_generale >= 12.0) {
        printf("\033[1;33m%.2f/20 - Assez bien 👌\033[0m\n", eleve->moyenne_generale);
    } else if (eleve->moyenne_generale >= 10.0) {
        printf("\033[1;33m%.2f/20 - Passable ⚠️\033[0m\n", eleve->moyenne_generale);
    } else {
        printf("\033[1;31m%.2f/20 - Insuffisant ⚠️\033[0m\n", eleve->moyenne_generale);
    }
    
    printf("═════════════════════════════════════════════════════════════════\n\n");
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
    
    // Supprimer espaces au début
    char* debut = str;
    while (*debut == ' ' || *debut == '\t' || *debut == '\n' || *debut == '\r') {
        debut++;
    }
    
    if (debut != str) {
        memmove(str, debut, strlen(debut) + 1);
    }
    
    // Supprimer espaces à la fin
    int len = strlen(str);
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\t' || 
                       str[len-1] == '\n' || str[len-1] == '\r')) {
        str[len-1] = '\0';
        len--;
    }
}