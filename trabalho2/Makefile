CXX := g++
CXXFLAGS := -O2 -Wall -std=c++11

# Detect OpenCV (prefer opencv4, fallback to opencv)
OPENCV_PKG := $(shell pkg-config --exists opencv4 && echo opencv4 || (pkg-config --exists opencv && echo opencv || echo ""))
ifeq ($(OPENCV_PKG),)
$(error OpenCV not found via pkg-config. Install opencv4/opencv dev packages with pkg-config files)
endif

OPENCV_CFLAGS := $(shell pkg-config --cflags $(OPENCV_PKG))
OPENCV_LIBS := $(shell pkg-config --libs $(OPENCV_PKG))

# Add OpenCV flags to standard vars
CXXFLAGS += $(OPENCV_CFLAGS)
LDLIBS := $(OPENCV_LIBS)

BIN_DIR := bin
# Derive binaries from sources
SRCS := ex1.cpp ex2.cpp
PROGS := $(SRCS:%.cpp=$(BIN_DIR)/%)

.PHONY: all clean ex1 ex2

all: $(PROGS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Single pattern rule
$(BIN_DIR)/%: %.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDLIBS)

ex1: $(BIN_DIR)/ex1
ex2: $(BIN_DIR)/ex2

clean:
	rm -rf build $(BIN_DIR)
