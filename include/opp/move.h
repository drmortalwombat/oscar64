#ifndef OPP_MOVE_H
#define OPP_MOVE_H

template <class T>
T && move(T & m)
{
	return (T &&)m;
}

#endif
