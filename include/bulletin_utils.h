// include/bulletin_utils.h
#ifndef BULLETIN_UTILS_H
#define BULLETIN_UTILS_H

#include "bulletin.h"

// Déplace la struct ici (extrait de bulletin_utils.c)
typedef struct {
    int semestre;
    char type[10];           // "MCP" ou "CP"
    char code[50];
    char module_parent[50];
    char nom[MAX_MATIERE];
    int est_module;
} MetadataMatiere;

// Prototypes
void extraire_metadata_matiere(const char* nom_complet, MetadataMatiere* meta);
void afficher_eleve(const Eleve* eleve);
void calculer_moyenne(Eleve* eleve);
void nettoyer_chaine(char* str);

#endif