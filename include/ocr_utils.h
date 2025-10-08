#ifndef OCR_UTILS_H
#define OCR_UTILS_H

#include <tesseract/capi.h>
#include <leptonica/allheaders.h>
#include <sqlite3.h>
#include "bulletin.h"

char* extraire_texte_image(const char* chemin_image);
int analyser_bulletin_texte(const char* texte_ocr, Eleve* eleve);
float extraire_note_depuis_ligne(const char* ligne);
void saisir_bulletin_manuel(sqlite3 *db);

#endif