NAME := webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++11
INCLUDE = -Iinclude

SRC := $(shell find src -type f -name '*.cpp')
OBJ := $(SRC:.cpp=.o)

TEST_DIR := test_files
TEST_BIN_DIR := tests/bin

UNIT_ROUTING_UTIL_BIN := $(TEST_BIN_DIR)/test_routing_utils

### add more unit test bins in here
UNIT_TEST_BINS := \
	$(UNIT_ROUTING_UTIL_BIN) 

$(UNIT_ROUTING_UTIL_BIN): $(TEST_DIR)/test_routing_utils.cpp src/RoutingUtils.cpp include/RoutingUtils.hpp include/Config.hpp
	mkdir -p $(TEST_BIN_DIR)
	$(CXX) $(INCLUDE) $(CXXFLAGS) $(TEST_DIR)/test_routing_utils.cpp src/RoutingUtils.cpp -o $@

test-routing-utils: $(UNIT_ROUTING_UTIL_BIN)
	./$(UNIT_ROUTING_UTIL_BIN)

######## ADD UNIT TESTS #############
test-unit: test-routing-utils

#### ADD OTHER LAYER OF TEST ####
test: test-unit

clean-tests:
	rm -rf $(TEST_BIN_DIR)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(NAME)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

clean: clean-tests
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re test test-unit test-routing-utils clean-tests 
