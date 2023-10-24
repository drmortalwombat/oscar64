#ifndef OPP_SLAB_H
#define OPP_SLAB_H

template<class T, int N>
class slabptr
{
public:
	char	index;

	slabptr(void)
		: index(N)
		{}

	slabptr(char i)
		: index(i)
		{}

	slabptr(const slabptr & i)
		: index(i.index)
		{}

	auto operator-> ();
	auto & operator* ();
};

template <class T, int N>
class slab
{
protected:
	static __striped	T	buffer[N];
	static	char		head;
	static	char		next[N];

public:
	typedef slabptr<T, N>	ptr;

	static void init(void);
	static auto alloc(void);
	static void free(ptr p);
};


template<class T, int N>
inline auto slabptr<T, N>::operator-> ()
{
	return slab<T, N>::buffer + index;
}

template<class T, int N>
inline auto & slabptr<T, N>::operator* ()
{
	return slab<T, N>::buffer[index];	
}


template <class T, int N>
void slab<T, N>::init(void)
{
	head = 0;
	for(char i=0; i<N; i++)
		next[i] = i + 1;
}

template <class T, int N>
auto slab<T, N>::alloc(void)
{
	char i = head;
	head = next[head];	
	return slabptr<T, N>(i);	
}

template <class T, int N>
void slab<T, N>::free(slabptr<T, N> p)
{
	next[p.index] = head;
	head = p.index;
}

#endif

