#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include "bulletin.h"

// Gestion de la base de données
int ouvrir_base(sqlite3 **db);
int creer_tables(sqlite3 *db);

// Opérations CRUD
int inserer_eleve(sqlite3 *db, Eleve *eleve);
Eleve* recuperer_eleve(sqlite3 *db, int id);
Eleve* rechercher_par_matricule(sqlite3 *db, const char* matricule);
void lister_tous_eleves(sqlite3 *db);

// Statistiques
void afficher_statistiques_eleve(sqlite3 *db, int eleve_id);

#endif