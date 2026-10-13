#include "Client.hpp"

ssize_t		Client::getWriteOffSet() const
{
	return _writeOffSet;
}

void		Client::setWriteOffSet(ssize_t byte)
{
	_writeOffSet += byte;
}