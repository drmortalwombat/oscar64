#ifndef HASHMAP_H
#define HASHMAP_H

#include <opp/list.h>
#include <opp/string.h>

namespace opp {

template<class T>
struct Hash
{};

template<>
struct Hash<int> {
	unsigned operator()(const int & k)
	{
		return k;
	}
};

template<>
struct Hash<string> {
	unsigned operator()(const string & k)
	{
		const char * cp = k.begin();
		if (cp)
		{
			unsigned	hash = 0;
			while (*cp)
				hash = hash * 17 + *cp++;
			return hash;
		}
		else			
			return 0;
	}
};

template <class K, class T>
struct hashpair
{
	K			key;
	T 			value;
	unsigned	hash;

	hashpair(void) {}
	hashpair(const K & k, unsigned h) : key(k), hash(h) {}
	hashpair(const hashpair<K, T> & p)
		: key(p.key), value(p.value), hash(p.hash) {}
};

template<class K, class T>
class hashmap
{
public:
	hashmap(void);
	~hashmap(void);

	typedef hashpair<K, T>			N;
	typedef list_iterator<N>	I;

	T & at(const K & key);

	void insert(const K & key, const T & value);
	void erase(const K & key);
	list_iterator<hashpair<K, T> > erase(list_iterator<hashpair<K, T> > iter);

	I begin(void);
	I end(void);
	I find(const K & key);

private:
	unsigned		mapsize;
	unsigned		size;

	list<N>			list;
	I					*	map;
};


template<class K, class T>
hashmap<K, T>::hashmap(void)
	: mapsize(0), size(0), map(nullptr)
	{}


template<class K, class T>
hashmap<K, T>::~hashmap(void)
{
	delete[] map;
}

template<class K, class T>
hashmap<K, T>::I hashmap<K, T>::begin(void)
{
	return list.begin();
}

template<class K, class T>
hashmap<K, T>::I hashmap<K, T>::end(void)
{
	return list.end();
}

template<class K, class T>
T & hashmap<K, T>::at(const K & key)
{
	if (mapsize == 0)
	{
		mapsize = 16;
		map = new I[16];
		for(unsigned i=0; i<16; i++)
			map[i] = list.end();
	}

	unsigned	m = mapsize - 1;
	unsigned	h = Hash<K>()(key);
	unsigned	hi = h & m;

	I 	p = map[hi];
	while (p != list.end() && (p->hash & m) == hi)
	{
		if (p->key == key) return p->value;
		p++;
	}

	p = list.insert(map[hi], N(key, h));
	map[hi] = p;
	return p->value;
}

template<class K, class T>
hashmap<K, T>::I hashmap<K, T>::find(const K & key)
{
	if (mapsize)
	{
		unsigned	m = mapsize - 1;
		unsigned	h = Hash<K>()(key);
		unsigned	hi = h & m;

		I 	p = map[hi];
		while (p != list.end() && (p->hash & m) == hi)
		{
			if (p->key == key) return p;
			p++;
		}		
	}	

	return list.end();
}


template<class K, class T>
void hashmap<K, T>::insert(const K & key, const T & value)
{
	this->at(key) = value;
}

template<class K, class T>
void hashmap<K, T>::erase(const K & key)
{
	erase(find(key));
}

template<class K, class T>
list_iterator<hashpair<K, T> > hashmap<K, T>::erase(list_iterator<hashpair<K, T> > iter)
{
	if (iter != list.end())
	{
		unsigned	hi = iter->hash & (mapsize - 1);
		bool		first = iter == map[hi];
		iter = list.erase(iter);
		if (first)
			map[hi] = iter;
	}
	return iter;
}

}

#endif
