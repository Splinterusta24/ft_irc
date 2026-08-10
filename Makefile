NAME        = ircserv
CXX         = c++
CXXFLAGS    = -Wall -Wextra -Werror -std=c++98 -I include

SRCS        = src/main.cpp \
              src/Server.cpp \
              src/ServerUtils.cpp \
              src/Client.cpp \
              src/Channel.cpp \
              src/Parser.cpp \
              src/Utils.cpp \
              src/CommandsAuth.cpp \
              src/CommandsChannel.cpp \
              src/CommandsMode.cpp \
              src/CommandsMessage.cpp

OBJS        = $(SRCS:.cpp=.o)

HEADERS     = include/Server.hpp \
              include/Client.hpp \
              include/Channel.hpp \
              include/Parser.hpp \
              include/Utils.hpp \
              include/Replies.hpp

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

# irssi davranışını taklit eden otomatik test paketi
test: $(NAME)
	@./tests/run_tests.sh

.PHONY: all clean fclean re test
