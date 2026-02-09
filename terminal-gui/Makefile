# TerminalGUI Makefile
# Suporta Linux (nativo) e preparado para Windows (futuro)

# Detecta plataforma automaticamente
ifeq ($(OS),Windows_NT)
    PLATFORM := windows
    EXE_EXT := .exe
    MKDIR := mkdir
    RM := del /Q
    RMDIR := rmdir /S /Q
else
    PLATFORM := linux
    EXE_EXT :=
    MKDIR := mkdir -p
    RM := rm -f
    RMDIR := rm -rf
endif

# Permite override: make PLATFORM=windows (para cross-compile)
ifdef TARGET
    PLATFORM := $(TARGET)
endif

# Configurações do compilador
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
INCLUDES = -I./include

# Cross-compilation para Windows (usando MinGW)
ifeq ($(PLATFORM),windows)
    CXX = x86_64-w64-mingw32-g++
    EXE_EXT = .exe
    CXXFLAGS += -DTGUI_WINDOWS
endif

ifeq ($(PLATFORM),linux)
    CXXFLAGS += -DTGUI_LINUX
endif

BIN_DIR = bin

EXAMPLES = basic dashboard perf_test

.PHONY: all clean $(EXAMPLES) info

# Mostra info da plataforma
info:
	@echo "Plataforma: $(PLATFORM)"
	@echo "Compilador: $(CXX)"
	@echo "Extensao:   $(EXE_EXT)"

all: $(BIN_DIR) $(EXAMPLES)
	@echo "Build completo para $(PLATFORM)"

$(BIN_DIR):
	$(MKDIR) $(BIN_DIR)

basic: $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) examples/basic.cpp -o $(BIN_DIR)/basic$(EXE_EXT)

dashboard: $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) examples/dashboard.cpp -o $(BIN_DIR)/dashboard$(EXE_EXT)

perf_test: $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) examples/perf_test.cpp -o $(BIN_DIR)/perf_test$(EXE_EXT)

clean:
ifeq ($(PLATFORM),windows)
	-$(RMDIR) $(BIN_DIR) 2>nul || true
else
	$(RMDIR) $(BIN_DIR)
endif

# Testes rápidos (só Linux)
test: all
ifeq ($(PLATFORM),linux)
	@echo "Rodando exemplos..."
	@timeout 3 ./$(BIN_DIR)/basic || true
	@timeout 3 ./$(BIN_DIR)/dashboard || true
	@echo "Testes finalizados"
else
	@echo "Testes automaticos disponiveis apenas no Linux"
endif
