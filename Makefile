NAME := webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++11
INCLUDE = -Iinclude

SRC := $(shell find src -type f -name '*.cpp')
OBJ := $(SRC:.cpp=.o)

TEST_DIR := test_files
TEST_BIN_DIR := tests/bin

UNIT_ROUTING_UTIL_BIN := $(TEST_BIN_DIR)/test_routing_utils
UNIT_URI_UTIL_BIN := $(TEST_BIN_DIR)/test_uri_utils
UNIT_REQUEST_PARSER_BIN := $(TEST_BIN_DIR)/test_request_parser

COMPO_ROUTING_BIN := $(TEST_BIN_DIR)/test_component_routing

### add more unit test bins in here
UNIT_TEST_BINS := \
	$(UNIT_ROUTING_UTIL_BIN) \
	$(UNIT_URI_UTIL_BIN) \
	$(UNIT_REQUEST_PARSER_BIN)

COMPO_TEST_BIN := \
	$(COMPO_ROUTING_BIN)

## ---- Targets ---- ##
all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(NAME)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

$(UNIT_ROUTING_UTIL_BIN): \
		$(TEST_DIR)/test_routing_utils.cpp \
		$(TEST_DIR)/TestRunner.hpp \
		src/RoutingUtils.cpp 
	mkdir -p $(TEST_BIN_DIR)
	$(CXX) $(INCLUDE) $(CXXFLAGS) $(TEST_DIR)/test_routing_utils.cpp src/RoutingUtils.cpp -o $@

$(UNIT_URI_UTIL_BIN): \
		$(TEST_DIR)/test_uri_utils.cpp \
		$(TEST_DIR)/TestRunner.hpp \
		src/UriUtils.cpp \
		src/Http.cpp
	mkdir -p $(TEST_BIN_DIR)
	$(CXX) $(INCLUDE) $(CXXFLAGS) $(TEST_DIR)/test_uri_utils.cpp src/UriUtils.cpp src/Http.cpp -o $@

$(UNIT_REQUEST_PARSER_BIN): \
		$(TEST_DIR)/test_request_parser.cpp \
		$(TEST_DIR)/TestRunner.hpp \
		src/RequestParser.cpp \
		src/UriUtils.cpp \
		src/Http.cpp
	mkdir -p $(TEST_BIN_DIR)
	$(CXX) $(INCLUDE) $(CXXFLAGS) $(TEST_DIR)/test_request_parser.cpp src/RequestParser.cpp src/UriUtils.cpp src/Http.cpp -o $@

$(COMPO_ROUTING_BIN): \
		$(TEST_DIR)/test_component_routing/test_routing.cpp \
		$(TEST_DIR)/TestRunner.hpp \
		src/ClientRouting.cpp \
		src/Client.cpp \
		src/RequestParser.cpp \
		src/RoutingUtils.cpp \
		src/ResponseBuilder.cpp \
		src/Logger.cpp \
		src/UriUtils.cpp 
	mkdir -p $(TEST_BIN_DIR)
	$(CXX) $(INCLUDE) $(CXXFLAGS) $(TEST_DIR)/test_component_routing/test_routing.cpp src/ClientRouting.cpp src/Client.cpp src/RequestParser.cpp src/RoutingUtils.cpp src/ResponseBuilder.cpp src/Logger.cpp src/UriUtils.cpp -o $@


test-routing-utils: $(UNIT_ROUTING_UTIL_BIN)
	./$(UNIT_ROUTING_UTIL_BIN)


test-uri-utils: $(UNIT_URI_UTIL_BIN)
	./$(UNIT_URI_UTIL_BIN)

test-request-parser: $(UNIT_REQUEST_PARSER_BIN)
	./$(UNIT_REQUEST_PARSER_BIN)

test-component-routing: $(COMPO_ROUTING_BIN)
	./$(COMPO_ROUTING_BIN)

######## ADD UNIT TESTS #############
test-unit: test-routing-utils test-uri-utils test-request-parser

test-component: test-component-routing

#### ADD OTHER LAYER OF TEST ####
test: test-unit test-component

clean-tests:
	rm -rf $(TEST_BIN_DIR)

clean: clean-tests
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re test test-unit test-routing-utils test-uri-utils test-request-parser clean-tests test_component_routing
