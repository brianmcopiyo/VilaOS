# Windows WebView2 host. Shared shell kernel lives in ../src.
ROOT     = ..
SHARED   = $(ROOT)/src
SRC_DIR  = src
BIN_DIR  = bin
TARGET   = $(BIN_DIR)/shell.exe

CC       = gcc
CXX      = g++
CFLAGS   = -Wall -Wextra -std=c11 -I$(SRC_DIR) -I$(SHARED)
CXXFLAGS = -Wall -Wextra -std=c++17 -I$(SRC_DIR) -I$(SHARED) \
           -Ilibs/webview -Ilibs/webview2/build/native/include -DWEBVIEW_STATIC
LDFLAGS  = -mwindows -Llibs/webview2/build/native/x64 -lWebView2Loader.dll \
           -lole32 -loleaut32 -lstdc++ -lgdi32 -luser32 -lshlwapi -lversion -ladvapi32

COBJS = $(BIN_DIR)/main.o \
        $(BIN_DIR)/parser.o \
        $(BIN_DIR)/builtins.o \
        $(BIN_DIR)/exec.o \
        $(BIN_DIR)/util.o

CXXOBJS = $(BIN_DIR)/native.o

.PHONY: all clean run ui

all: ui $(TARGET)

ui:
	cd $(ROOT) && cmd /C "set UI_TARGET=win&& npm run build:ui"
	if not exist $(BIN_DIR) mkdir $(BIN_DIR)
	copy /Y libs\webview2\build\native\x64\WebView2Loader.dll $(BIN_DIR)\WebView2Loader.dll

$(BIN_DIR):
	if not exist $(BIN_DIR) mkdir $(BIN_DIR)

$(BIN_DIR)/main.o: $(SRC_DIR)/main.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/main.c -o $(BIN_DIR)/main.o

$(BIN_DIR)/parser.o: $(SHARED)/parser.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -c $(SHARED)/parser.c -o $(BIN_DIR)/parser.o

$(BIN_DIR)/builtins.o: $(SHARED)/builtins.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -c $(SHARED)/builtins.c -o $(BIN_DIR)/builtins.o

$(BIN_DIR)/exec.o: $(SHARED)/exec.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -c $(SHARED)/exec.c -o $(BIN_DIR)/exec.o

$(BIN_DIR)/util.o: $(SHARED)/util.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -c $(SHARED)/util.c -o $(BIN_DIR)/util.o

$(BIN_DIR)/native.o: $(SRC_DIR)/native.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -c $(SRC_DIR)/native.cpp -o $(BIN_DIR)/native.o

$(TARGET): $(COBJS) $(CXXOBJS)
	$(CXX) $(COBJS) $(CXXOBJS) -o $(TARGET) $(LDFLAGS)

run: all
	$(TARGET)

clean:
	if exist $(BIN_DIR)\*.o del /Q $(BIN_DIR)\*.o
	if exist $(TARGET) del /Q $(TARGET)
