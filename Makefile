NAME        = ircserv
CXX         = c++
CXXFLAGS    = -Wall -Wextra -Werror -std=c++98 -I include

SRCS        = src/main.cpp \
              src/Server.cpp \
              src/Client.cpp \
              src/Utils.cpp \
              src/Parser.cpp \
              src/Channel.cpp \
              src/Commands.cpp
              
OBJS        = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
