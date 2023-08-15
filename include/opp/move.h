#ifndef OPP_MOVE_H
#define OPP_MOVE_H

namespace opp {

template <class T>
T && move(T & m)
{
	return (T &&)m;
}

}

#endif
