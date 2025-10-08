#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include "bulletin.h"

int ouvrir_base(sqlite3 **db);
int creer_tables(sqlite3 *db);
int inserer_eleve(sqlite3 *db, Eleve *eleve);
Eleve* recuperer_eleve(sqlite3 *db, int id);
void lister_tous_eleves(sqlite3 *db);

#endif