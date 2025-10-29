CC = gcc
CFLAGS = -Wall -g -Iinclude `pkg-config --cflags gtk+-3.0`
LDFLAGS = -Wl,-rpath=/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu -rdynamic
LIBS = -lsqlite3 -ltesseract -llept `pkg-config --libs gtk+-3.0`

SRC_DIR = src
BUILD_DIR = build
DATA_DIR = data
UI_DIR = ui

TARGET = bulletin_scanner

SOURCES = $(SRC_DIR)/main.c \
          $(SRC_DIR)/gui.c \
          $(SRC_DIR)/bulletin_utils.c \
          $(SRC_DIR)/database.c \
          $(SRC_DIR)/ocr_utils.c

OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

all: dirs $(TARGET)

dirs:
	@mkdir -p $(BUILD_DIR) $(DATA_DIR) $(UI_DIR)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo "✅ Compilation terminée"
	@echo ""
	@echo "Pour lancer l'application, utilisez:"
	@echo "  make run"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "🧹 Nettoyage..."
	rm -rf $(BUILD_DIR) $(TARGET)
	@echo "Propre !"

run: $(TARGET)
	@env -i \
		PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
		LD_LIBRARY_PATH=/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu \
		DISPLAY=$(DISPLAY) \
		HOME=$(HOME) \
		XAUTHORITY=$(XAUTHORITY) \
		./$(TARGET)

.PHONY: all clean run dirs