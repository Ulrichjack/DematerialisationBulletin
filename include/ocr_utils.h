#ifndef OCR_UTILS_H
#define OCR_UTILS_H

#include <tesseract/capi.h>
#include <leptonica/allheaders.h>
#include <sqlite3.h>
#include "bulletin.h"

// Extraction de texte
char* extraire_texte_image(const char* chemin_image);

// Analyse du bulletin
int analyser_bulletin_texte(const char* texte_ocr, Eleve* eleve);

// Extraction de notes et coefficients
float extraire_note_depuis_ligne(const char* ligne);
float extraire_coefficient(const char* ligne);

// Extraction d'informations structurées
void extraire_info_apres_label(const char* texte, const char* label, char* destination, int max_len);

// Détection de lignes de matières
int est_code_matiere(const char* token);
int est_ligne_matiere_tableau(const char* ligne);

// Saisie manuelle
void saisir_bulletin_manuel(sqlite3 *db);

#endif