CC = gcc
CFLAGS = -Wall -g -Iinclude 
LIBS = -lsqlite3 -ltesseract -llept

SRC_DIR = src
BUILD_DIR = build
DATA_DIR = data

TARGET = bulletin_scanner

SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/bulletin_utils.c $(SRC_DIR)/database.c $(SRC_DIR)/ocr_utils.c

OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
	@echo "✅ Compilation terminée. Exécutable créé : $(TARGET)"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "🧹 Nettoyage du projet..."
	rm -f $(BUILD_DIR)/*.o $(TARGET)
	@echo "Fichiers compilés supprimés."

.PHONY: all clean