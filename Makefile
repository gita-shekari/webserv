NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++11

SRC = src/main.cpp \
      src/Server.cpp \
      src/Config.cpp \
	  src/Client.cpp \
	  src/RequestParser.cpp \
	  src/Request.cpp

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
