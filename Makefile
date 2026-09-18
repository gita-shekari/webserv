NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++11

SRC = \
	src/Client.cpp \
	src/ConfigParser.cpp \
	src/Http.cpp \
	src/Logger.cpp \
	src/RequestParser.cpp \
	src/ResponseBuilder.cpp \
	src/Server.cpp \
	src/main.cpp
#	test_files/server_nonblocking.cpp

OBJ = $(SRC:.cpp=.o)

INCLUDE = -Iinclude

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(NAME)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re