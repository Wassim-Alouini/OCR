# === Makefile global ===

CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS_GRAPHICS = `sdl2-config --cflags --libs` -lSDL2_ttf -lm

# Dossiers
SOLVER_DIR = Solver
GRAPHICS_DIR = Graphics
NEURAL_DIR = NeuralNetwork

# Cibles
SOLVER_TARGET = solvers
GRAPHICS_TARGET = ocr_epita
NEURAL_TARGET = neural

# Sources
SOLVER_SRCS = $(SOLVER_DIR)/solver.c
GRAPHICS_SRCS = $(GRAPHICS_DIR)/main.c \
                $(GRAPHICS_DIR)/image_loader.c \
                $(GRAPHICS_DIR)/window_manager.c \
                $(GRAPHICS_DIR)/cmd_window.c \
                $(GRAPHICS_DIR)/bounds.c\
				$(GRAPHICS_DIR)/rot.c\
		$(GRAPHICS_DIR)/detectgrid.c\
		$ Solver/solver.c\
                $(NEURAL_DIR)/neuralnetwork.c\
                $(GRAPHICS_DIR)/preprocess.c
NEURAL_SRCS = $(NEURAL_DIR)/main.c \
              $(NEURAL_DIR)/neuralnetwork.c

# Règles principales
all: ocr 

solver: $(SOLVER_SRCS)
	@echo ">> Building solver..."
	$(CC) $(CFLAGS) -o $(SOLVER_TARGET) $(SOLVER_SRCS)

ocr: $(GRAPHICS_SRCS)
	@echo ">> Building graphics..."
	$(CC) $(CFLAGS) -o $(GRAPHICS_TARGET) $(GRAPHICS_SRCS) $(LDFLAGS_GRAPHICS)

neural: $(NEURAL_SRCS)
	@echo ">> Building neural network..."
	$(CC) $(CFLAGS) -o $(NEURAL_TARGET) $(NEURAL_SRCS) -lm

# Nettoyage
clean:
	@echo ">> Cleaning..."
	rm -f $(SOLVER_TARGET) $(GRAPHICS_TARGET) $(NEURAL_TARGET)
	find . -name "*.o" -type f -delete

# Reconstruction complète
re: clean all

.PHONY: all solver graphics neural clean re
