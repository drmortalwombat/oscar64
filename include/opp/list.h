#ifndef OPP_LIST
#define OPP_LIST

namespace opp 
{

template <class T>
class listhead
{
public:
	listnode<T>	*	succ, * pred;	

	listhead()
	{
		succ = (listnode<T>	*)this;
		pred = (listnode<T>	*)this;
	}
};

template <class T>
class listnode : public listhead<T>
{
public:
	T				data;

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

	T * operator->()
	{
		return &(node->data);
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
	typedef listnode<T>	ln;
	listhead<T>	head;
public:
	typedef T 					element_type;
	typedef list_iterator<T>	iterator_type;

	list(void) 
	{}

	list(const list & l);

	list(list && l)
	{
		head.succ = l.head.succ;
		head.pred = l.head.pred;
		head.succ->pred = head;
		head.pred->succ = head;
		l.head.succ = (listnode<T>	*)&(l.head);
		l.head.pred = (listnode<T>	*)&(l.head);
	}

	list & operator=(const list & l);

	list & operator=(list && l)
	{
		head.succ = l.head.succ;
		head.pred = l.head.pred;
		head.succ->pred = head;
		head.pred->succ = head;
		l.head.succ = (listnode<T>	*)&(l.head);
		l.head.pred = (listnode<T>	*)&(l.head);
		return *this;
	}

	~list(void)
	{
		listnode<T>	*	n = head.succ;
		while (n != &head)
		{
			listnode<T>	*	m = n->succ;
			delete n;
			n = m;
		}
	}

	list_iterator<T> begin(void)
	{
		return list_iterator<T>(head.succ);
	}

	list_iterator<T> end(void)
	{
		return list_iterator<T>((listnode<T> *)&head);
	}

	T & front(void)
	{
		return head.succ->data;
	}

	const T & front(void) const
	{
		return head.succ->data;
	}

	T & back(void)
	{
		return head.pred->data;
	}

	const T & back(void) const
	{
		return head.pred->data;
	}

	list_iterator<T> erase(list_iterator<T> it);

	list_iterator<T> erase(list_iterator<T> first, list_iterator<T> last);

	void pop_front(void);

	void pop_back(void);

	void push_front(const T & t);

	void push_front(T && t);

	void push_back(const T & t);

	void push_back(T && t);

	void clear(void);

	void append(const list & l);

	list_iterator<T> insert(list_iterator<T> it, const T & t);

	list_iterator<T> insert(list_iterator<T> it, T && t);
};

template <class T>
list<T>::list(const list<T> & l)
{
	append(l);
}

template <class T>
list<T> & list<T>::operator=(const list<T> & l)
{
	if (&l != this)
	{
		clear();
		append(l);
	}
	return *this;
}

template <class T>
void list<T>::pop_front(void)
{
	listnode<T>	*	n = head.succ;
	head.succ = n->succ;
	n->succ->pred = (listnode<T> *)&head;
	delete n;
}

template <class T>
void list<T>::pop_back(void)
{
	listnode<T>	*	n = head.pred;
	head.pred = n->pred;
	n->pred->succ = (listnode<T> *)&head;
	delete n;
}

template <class T>
void list<T>::push_front(const T & t)
{
	listnode<T>	*	n = new listnode<T>(t);
	n->pred = (listnode<T> *)&head;
	n->succ = head.succ;
	head.succ->pred = n;
	head.succ = n;
}

template <class T>
void list<T>::push_front(T && t)
{
	listnode<T>	*	n = new listnode<T>(t);
	n->pred = (listnode<T> *)&head;
	n->succ = head.succ;
	head.succ->pred = n;
	head.succ = n;
}

template <class T>
void list<T>::push_back(const T & t)
{
	listnode<T>	*	n = new listnode<T>(t);
	n->succ = (listnode<T> *)&head;
	n->pred = head.pred;
	head.pred->succ = n;
	head.pred = n;
}

template <class T>
void list<T>::push_back(T && t)
{
	listnode<T>	*	n = new listnode<T>(t);
	n->succ = (listnode<T> *)&head;
	n->pred = head.pred;
	head.pred->succ = n;
	head.pred = n;
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


template <class T>
void list<T>::clear(void)
{
	listnode<T>	*	n = head.succ;
	while (n != &head)
	{
		listnode<T>	*	m = n->succ;
		delete n;
		n = m;
	}
	head.succ = (listnode<T> *)&head;
	head.pred = (listnode<T> *)&head;
}

template <class T>
void list<T>::append(const list<T> & l)
{
	listnode<T>	*	n = l.head.succ;
	while (n != &(l.head))
	{
		push_back(n->data);
		n = n->succ;
	}
}

}

#endif
