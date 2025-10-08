# 📚 Guide Étape par Étape - Scanner de Bulletins (avec SQLite)

## 🎯 Vision du Projet
Tu vas créer une appli qui prend une photo/scan d'un bulletin papier et le transforme en données numériques stockées dans une **base de données SQLite**.

---

## ✅ Étape 1 : Structures de Données (DÉJÀ FAIT ✓)

**Ce que t'as déjà :**
- Les structures C (Eleve, Matiere, BaseDonnees)
- Des fonctions pour créer, ajouter et afficher des élèves en mémoire
- Un système qui marche en RAM

**Résultat :** Tu peux créer des élèves et les manipuler en mémoire.

---

## 🗄️ Étape 2 : Base de Données SQLite (À FAIRE EN PREMIER)

### Pourquoi SQLite ?
- Base de données légère (un simple fichier .db)
- Pas besoin de serveur
- Très utilisé dans les applis C
- Requêtes SQL simples et puissantes

### Ce que tu dois accomplir :

#### 2.1 Installer SQLite
**Objectif :** Avoir SQLite sur ton système

**Sur Linux :**
```bash
sudo apt-get install sqlite3 libsqlite3-dev
```

**Sur macOS :**
```bash
brew install sqlite3
```

**Test :** Tape `sqlite3 --version` dans le terminal

#### 2.2 Concevoir ta base de données
**Objectif :** Créer la structure de tes tables

**Tu vas créer 2 tables :**

**Table 1 : eleves**
- id (clé primaire, auto-increment)
- nom
- prenom
- classe
- date_bulletin
- moyenne_generale

**Table 2 : matieres**
- id (clé primaire, auto-increment)
- eleve_id (clé étrangère vers eleves)
- nom_matiere
- note
- coefficient
- appreciation

**Pourquoi 2 tables ?** Parce qu'un élève a plusieurs matières. C'est une relation "1 vers plusieurs".

#### 2.3 Créer les fonctions SQLite de base
**Objectif :** Pouvoir interagir avec la base

**Fonctions à créer :**

1. **Initialiser la base**
   - Ouvrir/créer le fichier bulletins.db
   - Créer les tables si elles n'existent pas
   - Retourner la connexion

2. **Fermer la base**
   - Fermer proprement la connexion
   - Libérer la mémoire

3. **Insérer un élève**
   - Ajouter l'élève dans la table eleves
   - Récupérer son ID auto-généré
   - Insérer toutes ses matières dans la table matieres

4. **Récupérer un élève**
   - Chercher par nom/prénom ou ID
   - Faire un JOIN pour récupérer ses matières
   - Remplir la structure Eleve

5. **Lister tous les élèves**
   - SELECT sur la table eleves
   - Afficher la liste

6. **Mettre à jour un élève**
   - Modifier les infos d'un élève existant

7. **Supprimer un élève**
   - Effacer l'élève ET ses matières (CASCADE)

#### 2.4 Modifier ton Makefile
**À ajouter :**
```
LIBS = -ltesseract -llept -lsqlite3
```

#### 2.5 Créer un nouveau fichier
**Créer `src/database.c`** avec toutes les fonctions SQLite

**Mettre à jour le Makefile :**
```
$(TARGET): $(BUILD_DIR)/main.o $(BUILD_DIR)/bulletin_utils.o $(BUILD_DIR)/database.o
```

#### 2.6 Tester la base
**Créer un test dans main.c :**
- Ouvrir la base
- Insérer 2-3 élèves
- Les récupérer
- Les afficher
- Fermer la base
- Vérifier avec `sqlite3 data/bulletins.db` que les données sont là

**Temps estimé :** 2-3 jours

---

## 🔧 Étape 3 : Fonctions de Calcul (APRÈS SQLite)

### Ce que tu dois accomplir :

#### 3.1 Fonctions mathématiques
**Objectif :** Faire des calculs sur les notes

**À créer :**

1. **Calculer la moyenne d'un élève**
   - Tu l'as déjà, mais maintenant elle doit mettre à jour la base
   - Après calcul, UPDATE dans la table eleves

2. **Extraire un nombre depuis du texte**
   - Pour l'OCR plus tard
   - Ex: "Note: 15.5/20" → 15.5

3. **Calculer le rang dans la classe**
   - Requête SQL pour compter combien ont mieux
   - Retourner le rang

4. **Statistiques de classe**
   - Moyenne de la classe (AVG en SQL)
   - Meilleure note (MAX)
   - Pire note (MIN)
   - Nombre d'élèves (COUNT)

**Astuce :** Utilise les fonctions SQL (AVG, MAX, MIN, COUNT) au lieu de tout faire en C.

#### 3.2 Fonctions de recherche avancée
**À créer :**

1. **Chercher par classe**
   - SELECT * FROM eleves WHERE classe = '3ème A'

2. **Chercher par période**
   - Tous les bulletins d'une date donnée

3. **Top 10 des meilleurs**
   - ORDER BY moyenne_generale DESC LIMIT 10

**Temps estimé :** 1-2 jours

---

## 📸 Étape 4 : OCR - Scanner les Images (JOUR 5-6)

### Ce que tu dois accomplir :

#### 4.1 Installation de Tesseract
**Objectif :** Installer le logiciel qui lit le texte dans les images

**À faire :**
- Installer Tesseract
- Télécharger le pack français
- Tester en ligne de commande

#### 4.2 Créer la fonction d'extraction
**Objectif :** Extraire le texte d'une image

**À créer : `src/ocr_utils.c`**

**Fonction principale :**
- Prendre le chemin de l'image
- Initialiser Tesseract
- Lire l'image avec Leptonica
- Extraire tout le texte
- Retourner le texte brut

#### 4.3 Analyser le texte OCR
**Objectif :** Transformer texte → structure Eleve

**À faire :**
- Chercher le nom (souvent en haut, en gras)
- Chercher la classe (ex: "Classe: 3ème A")
- Identifier les lignes avec les matières
- Pour chaque matière :
  - Extraire le nom
  - Extraire la note
  - Extraire le coefficient
  - Extraire l'appréciation

**Techniques :**
- Utiliser des mots-clés ("Nom:", "Classe:", "Matière:", etc.)
- Chercher les patterns (nombre entre 0 et 20 = note)
- Corriger les erreurs OCR (O → 0, l → 1)

#### 4.4 Pipeline complet
**Enchaînement :**
1. Prendre une image
2. Extraire le texte (OCR)
3. Analyser le texte
4. Remplir la structure Eleve
5. Calculer la moyenne
6. **Insérer dans SQLite**

**Temps estimé :** 2-3 jours

---

## 🖥️ Étape 5 : Interface Utilisateur (DERNIER JOUR)

### Ce que tu dois accomplir :

#### 5.1 Menu principal
**Options du menu :**

```
=== SCANNER DE BULLETINS ===
1. Scanner un nouveau bulletin (image)
2. Ajouter un bulletin manuellement
3. Chercher un élève
4. Afficher tous les élèves
5. Afficher une classe
6. Statistiques d'une classe
7. Top 10 des meilleurs
8. Exporter en CSV
9. Quitter
```

#### 5.2 Fonction : Scanner un bulletin
**Déroulement :**
- Demander le chemin de l'image
- Vérifier que le fichier existe
- Lancer l'OCR
- Afficher le texte extrait
- Analyser automatiquement
- Demander confirmation avant d'insérer
- Insérer dans SQLite
- Afficher le résultat

#### 5.3 Fonction : Ajouter manuellement
**Déroulement :**
- Demander nom, prénom, classe
- Demander le nombre de matières
- Pour chaque matière :
  - Demander nom, note, coefficient, appréciation
- Calculer la moyenne
- Insérer dans SQLite

#### 5.4 Fonction : Chercher un élève
**Déroulement :**
- Demander nom et prénom
- Faire la requête SQL
- Afficher le bulletin complet avec toutes les matières

#### 5.5 Fonction : Exporter en CSV
**Bonus utile :**
- Exporter toute la base en fichier CSV
- Format Excel compatible
- Utile pour analyses externes

**Temps estimé :** 1 jour

---

## 📅 Planning sur 7 Jours

| Jour | Tâche | Objectif |
|------|-------|----------|
| **J1-J3** | Étape 2 : SQLite | Base de données complète |
| **J4** | Étape 3 : Calculs | Fonctions mathématiques + stats |
| **J5-J6** | Étape 4 : OCR | Scanner les images |
| **J7** | Étape 5 : Interface | Menu et navigation |

---

## 🗂️ Structure finale de ton projet

```
bulletin_scanner/
├── build/              (fichiers compilés)
├── data/
│   └── bulletins.db    (base SQLite)
├── images/             (bulletins scannés)
├── include/
│   └── bulletin.h
├── src/
│   ├── main.c
│   ├── bulletin_utils.c
│   ├── database.c      (NOUVEAU)
│   └── ocr_utils.c     (NOUVEAU)
├── Makefile
└── README.md
```

---

## 🎯 Avantages de SQLite

✅ **Requêtes puissantes**
- Chercher par n'importe quel critère
- Trier facilement
- Statistiques en une ligne SQL

✅ **Pas de limite de taille**
- Des milliers de bulletins possibles

✅ **Sauvegarde automatique**
- Tout est dans bulletins.db
- Pas de risque de perte

✅ **Export facile**
- Vers Excel, CSV, etc.

✅ **Multi-utilisateurs** (futur)
- Plusieurs personnes peuvent scanner en même temps

---

## 💡 Ordre de Priorité

1. **D'ABORD SQLite** → C'est le cœur du projet
2. **PUIS les calculs** → Utilise SQL au maximum
3. **ENSUITE l'OCR** → La partie la plus complexe
4. **ENFIN l'interface** → Pour rendre tout utilisable

---

## 📝 Exemple de requêtes SQL utiles

**Insérer un élève :**
```sql
INSERT INTO eleves (nom, prenom, classe, date_bulletin, moyenne_generale) 
VALUES ('DUPONT', 'Jean', '3ème A', '2024-01-15', 14.5);
```

**Récupérer un élève avec ses matières :**
```sql
SELECT e.*, m.nom_matiere, m.note, m.coefficient 
FROM eleves e 
LEFT JOIN matieres m ON e.id = m.eleve_id 
WHERE e.nom = 'DUPONT' AND e.prenom = 'Jean';
```

**Top 5 de la classe :**
```sql
SELECT nom, prenom, moyenne_generale 
FROM eleves 
WHERE classe = '3ème A' 
ORDER BY moyenne_generale DESC 
LIMIT 5;
```

**Moyenne de classe :**
```sql
SELECT AVG(moyenne_generale) as moyenne_classe 
FROM eleves 
WHERE classe = '3ème A';
```

---

## ❓ Question pour toi

**Tu veux partir sur SQLite ou garder les fichiers texte ?**

**SQLite = Plus professionnel, plus puissant, mais un peu plus complexe**
**Fichiers texte = Plus simple mais limité**

Vu que t'as une semaine, je te conseille **SQLite** ! C'est le bon moment pour apprendre et ça rendra ton projet beaucoup plus impressionnant. 💪
