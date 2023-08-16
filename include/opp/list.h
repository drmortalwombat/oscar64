#ifndef OPP_LIST
#define OPP_LIST

namespace opp 
{

template <class T>
class listnode
{
public:
	listnode	*	succ, * pred;
	T				data;

	listnode()
	{
		succ = this;
		pred = this;
	}

	listnode(const T & t) : data(t) {}
	listnode(T && t) : data(t) {}
};

template <class T>
class list_iterator
{
public:
	listnode<T>	*	node;
public:
	list_iterator(void) : node(nullptr) {}
	list_iterator(listnode<T> * n) : node(n) {}
	list_iterator(const list_iterator & li) : node(li.node) {}
	list_iterator & operator=(const list_iterator & li)
	{
		node = li.node;
		return *this;
	}

	T & operator*()
	{
		return node->data;
	}

	list_iterator & operator++(void)
	{
		node = node->succ;
		return *this;
	}

	list_iterator operator++(int)
	{
		listnode<T>	* n = node;	
		node = node->succ;
		return list_iterator(n);
	}

	list_iterator & operator+=(int n)
	{
		while (n--)
			node = node->succ;	
		return *this;
	}

	list_iterator & operator--(void)
	{
		node = node->pred;
		return *this;
	}

	list_iterator operator++(int)
	{
		listnode<T>	* n = node;	
		node = node->pred;
		return list_iterator(n);
	}

	list_iterator & operator-=(int n)
	{
		while (n--)
			node = node->pred;	
		return *this;
	}

	bool operator==(const list_iterator & li)
	{
		return node == li.node;
	}

	bool operator!=(const list_iterator & li)
	{
		return node != li.node;
	}

};


template <class T>
class list
{
private:
	listnode<T>	node;
public:
	typedef T 					element_type;
	typedef list_iterator<T>	iterator_type;

	list(void) 
	{}

	~list(void)
	{
		listnode<T>	*	n = node.succ;
		while (n != &node)
		{
			listnode<T>	*	m = n->succ;
			delete n;
			n = m;
		}
	}

	list_iterator<T> begin(void)
	{
		return list_iterator<T>(node.succ);
	}

	list_iterator<T> end(void)
	{
		return list_iterator<T>(&node);
	}

	T & front(void)
	{
		return node.succ->data;
	}

	const T & front(void) const
	{
		return node.succ->data;
	}

	T & back(void)
	{
		return node.pred->data;
	}

	const T & back(void) const
	{
		return node.pred->data;
	}

	list_iterator<T> erase(list_iterator<T> it);

	list_iterator<T> erase(list_iterator<T> first, list_iterator<T> last);

	void pop_front(void);

	void pop_back(void);

	void push_front(const T & t);

	void push_front(T && t);

	void push_back(const T & t);

	void push_back(T && t);

	list_iterator<T> insert(list_iterator<T> it, const T & t);

	list_iterator<T> insert(list_iterator<T> it, T && t);
};

template <class T>
void list<T>::pop_front(void)
{
	listnode<T>	*	n = node.succ;
	node.succ = n->succ;
	n->succ->pred = &node;
	delete n;
}

template <class T>
void list<T>::pop_back(void)
{
	listnode<T>	*	n = node.pred;
	node.pred = n->pred;
	n->pred->succ = &node;
	delete n;
}

template <class T>
void list<T>::push_front(const T & t)
{
	listnode<T>	*	n = new listnode<T>(t);
	n->pred = &node;
	n->succ = node.succ;
	node.succ->pred = n;
	node.succ = n;
}

template <class T>
void list<T>::push_front(T && t)
{
	listnode<T>	*	n = new listnode<T>(t);
	n->pred = &node;
	n->succ = node.succ;
	node.succ->pred = n;
	node.succ = n;
}

template <class T>
void list<T>::push_back(const T & t)
{
	listnode<T>	*	n = new listnode<T>(t);
	n->succ = &node;
	n->pred = node.pred;
	node.pred->succ = n;
	node.pred = n;
}

template <class T>
void list<T>::push_back(T && t)
{
	listnode<T>	*	n = new listnode<T>(t);
	n->succ = &node;
	n->pred = node.pred;
	node.pred->succ = n;
	node.pred = n;
}


template <class T>
list_iterator<T> list<T>::erase(list_iterator<T> it)
{
	listnode<T>	*	n = it.node;
	listnode<T>	*	s = n->succ;

	n->succ->pred = n->pred;
	n->pred->succ = n->succ;
	delete n;

	return list_iterator<T>(s);
}

template <class T>
list_iterator<T> list<T>::erase(list_iterator<T> first, list_iterator<T> last)
{
	listnode<T>	*	n = first.node;
	listnode<T>	*	s = last.node;

	n->pred->succ = s;
	s->pred = n->pred;

	while (n != s)
	{
		listnode<T>	*	m = n->succ;
		delete n;
		n = m;
	}

	return list_iterator<T>(s);
}

template <class T>
list_iterator<T> list<T>::insert(list_iterator<T> it, const T & t)
{
	listnode<T>	*	n = new listnode<T>(t);
	n->succ = it.node;
	n->pred = it.node->pred;
	it.node->pred->succ = n;
	it.node->pred = n;
	return list_iterator<T>(n);
}

template <class T>
list_iterator<T> list<T>::insert(list_iterator<T> it, T && t)
{
	listnode<T>	*	n = new listnode<T>(t);
	n->succ = it.node;
	n->pred = it.node->pred;
	it.node->pred->succ = n;
	it.node->pred = n;
	return list_iterator<T>(n);
}

}

#endif
