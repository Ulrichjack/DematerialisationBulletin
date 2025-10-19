#ifndef BULLETIN_H
#define BULLETIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_NOM 300
#define MAX_PRENOM 300
#define MAX_CLASSE 50
#define MAX_MATIERE 200
#define MAX_MATIERES 200
#define MAX_PATH 256
#define MAX_MATRICULE 50
#define MAX_LIEU 100
#define MAX_NIVEAU 50
#define MAX_ANNEE 20

typedef struct {
    char nom_matiere[MAX_MATIERE];
    float note;
    float coefficient;
    char appreciation[200];
} Matiere;

typedef struct {
    int id;
    char nom[MAX_NOM];
    char prenom[MAX_PRENOM];
    char matricule[MAX_MATRICULE];        // Ajouté
    char classe[MAX_CLASSE];
    char niveau[MAX_NIVEAU];              // Ajouté (ex: "Niveau 1")
    char annee_academique[MAX_ANNEE];     // Ajouté (ex: "2023-2024")
    char lieu_naissance[MAX_LIEU];        // Ajouté
    char date_naissance[20];              // Ajouté
    char date_bulletin[20];
    Matiere matieres[MAX_MATIERES];
    int nombre_matieres;
    float moyenne_generale;
} Eleve;

void afficher_eleve(const Eleve* eleve);
void calculer_moyenne(Eleve* eleve);
void nettoyer_chaine(char* str);

#endif