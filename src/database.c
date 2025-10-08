#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include "bulletin.h"

int ouvrir_base(sqlite3 **db) {
    int rc = sqlite3_open("data/bulletins.db", db);
    if (rc) {
        fprintf(stderr, "Impossible d'ouvrir la base de données: %s\n", sqlite3_errmsg(*db));
        return 0;
    }
    return 1;
}

int creer_tables(sqlite3 *db) {
    const char *req_eleves =
        "CREATE TABLE IF NOT EXISTS eleves ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "nom TEXT, prenom TEXT, classe TEXT, date_bulletin TEXT, moyenne_generale REAL);";

    const char *req_matieres =
        "CREATE TABLE IF NOT EXISTS matieres ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "eleve_id INTEGER, nom_matiere TEXT, note REAL, coefficient REAL, appreciation TEXT, "
        "FOREIGN KEY (eleve_id) REFERENCES eleves(id) ON DELETE CASCADE);";

    char *err = NULL;
    int rc1 = sqlite3_exec(db, req_eleves, 0, 0, &err);
    int rc2 = sqlite3_exec(db, req_matieres, 0, 0, &err);

    if (rc1 != SQLITE_OK || rc2 != SQLITE_OK) {
        fprintf(stderr, "Erreur lors de la création des tables: %s\n", err);
        sqlite3_free(err);
        return 0;
    }
    return 1;
}

int inserer_eleve(sqlite3 *db, Eleve *eleve) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO eleves (nom, prenom, classe, date_bulletin, moyenne_generale) VALUES (?, ?, ?, ?, ?);";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, eleve->nom, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, eleve->prenom, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, eleve->classe, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, eleve->date_bulletin, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 5, eleve->moyenne_generale);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);

    int eleve_id = (int)sqlite3_last_insert_rowid(db);

    const char *sql2 = "INSERT INTO matieres (eleve_id, nom_matiere, note, coefficient, appreciation) VALUES (?, ?, ?, ?, ?);";
    for (int i = 0; i < eleve->nombre_matieres; i++) {
        if (sqlite3_prepare_v2(db, sql2, -1, &stmt, 0) != SQLITE_OK) return 0;
        sqlite3_bind_int(stmt, 1, eleve_id);
        sqlite3_bind_text(stmt, 2, eleve->matieres[i].nom_matiere, -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 3, eleve->matieres[i].note);
        sqlite3_bind_double(stmt, 4, eleve->matieres[i].coefficient);
        sqlite3_bind_text(stmt, 5, eleve->matieres[i].appreciation, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return 0;
        }
        sqlite3_finalize(stmt);
    }
    return 1;
}

Eleve* recuperer_eleve(sqlite3 *db, int id) {
    Eleve *eleve = malloc(sizeof(Eleve));
    if (!eleve) return NULL;
    
    const char *sql = "SELECT * FROM eleves WHERE id = ?;";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        free(eleve);
        return NULL;
    }
    
    sqlite3_bind_int(stmt, 1, id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        eleve->id = sqlite3_column_int(stmt, 0);
        strcpy(eleve->nom, (const char*)sqlite3_column_text(stmt, 1));
        strcpy(eleve->prenom, (const char*)sqlite3_column_text(stmt, 2));
        strcpy(eleve->classe, (const char*)sqlite3_column_text(stmt, 3));
        strcpy(eleve->date_bulletin, (const char*)sqlite3_column_text(stmt, 4));
        eleve->moyenne_generale = sqlite3_column_double(stmt, 5);
    } else {
        sqlite3_finalize(stmt);
        free(eleve);
        return NULL;
    }
    sqlite3_finalize(stmt);
    
    const char *sql2 = "SELECT * FROM matieres WHERE eleve_id = ?;";
    if (sqlite3_prepare_v2(db, sql2, -1, &stmt, 0) != SQLITE_OK) {
        free(eleve);
        return NULL;
    }
    
    sqlite3_bind_int(stmt, 1, id);
    eleve->nombre_matieres = 0;
    
    while (sqlite3_step(stmt) == SQLITE_ROW && eleve->nombre_matieres < MAX_MATIERES) {
        strcpy(eleve->matieres[eleve->nombre_matieres].nom_matiere, 
               (const char*)sqlite3_column_text(stmt, 2));
        eleve->matieres[eleve->nombre_matieres].note = sqlite3_column_double(stmt, 3);
        eleve->matieres[eleve->nombre_matieres].coefficient = sqlite3_column_double(stmt, 4);
        strcpy(eleve->matieres[eleve->nombre_matieres].appreciation, 
               (const char*)sqlite3_column_text(stmt, 5));
        eleve->nombre_matieres++;
    }
    sqlite3_finalize(stmt);
    
    return eleve;
}

void lister_tous_eleves(sqlite3 *db) {
    const char *sql = "SELECT id, nom, prenom, classe, moyenne_generale FROM eleves ORDER BY moyenne_generale DESC;";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "Erreur de requête\n");
        return;
    }
    
    printf("\n=== LISTE DES ÉLÈVES ===\n");
    printf("%-5s %-15s %-15s %-10s %s\n", "ID", "Nom", "Prénom", "Classe", "Moyenne");
    printf("-----------------------------------------------------------\n");
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("%-5d %-15s %-15s %-10s %.2f\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_text(stmt, 2),
               sqlite3_column_text(stmt, 3),
               sqlite3_column_double(stmt, 4));
    }
    
    sqlite3_finalize(stmt);
    printf("========================\n");
}