#ifndef OPP_BIDXLIST_H
#ifndef OPP_BIDXLIST_H

template <class T, int n>
class bindexlist
{
public:
	char		_free;
	char		_pred[n], _succ[n];
	T			_data[n];

	class iterator
	{
	public:
		bindexlist	*	_l;
		char			_i;

		iterator(void) : _l(nullptr) {}
		iterator(bindexlist * l, char i)
			: _l(l), _i(i) {}

		iterator(const iterator & li) : _l(li._l), _i(li._i) {}
		iterator & operator=(const iterator & li)
		{
			_l = li._l;
			_i = li._i;
			return *this;
		}

		T & operator*()
		{
			return _l->_data[_i];
		}

		T * operator->()
		{
			return _l->_data + _i;
		}

		iterator & operator++(void)
		{
			_i = _l->_succ[_i];
			return *this;
		}

		iterator operator++(int)
		{
			char i = _i;
			_i = _l->_succ[_i];
			return bindexlist::iterator(_l, i);
		}

		iterator & operator+=(char n)
		{
			while (n--)
				_i = _l->_succ[_i];
			return *this;
		}

		iterator & operator--(void)
		{
			_i = _l->_pred[_i];
			return *this;
		}

		iterator operator++(int)
		{
			char i = _i;
			_i = _l->_pred[_i];
			return bindexlist::iterator(_l, i);
		}

		iterator & operator-=(char n)
		{
			while (n--)
				_i = _l->_pred[_i];
			return *this;
		}

		bool operator==(const iterator & li)
		{
			return _i == li._i;
		}

		bool operator!=(const iterator & li)
		{
			return _i != li._i;
		}
	};

public:
	typedef T 			element_type;
	typedef iterator	iterator_type;

	bindexlist(void)
	{
		_succ[0] = 0;
		_pred[0] = 0;
		for(char i=1; i<n; i++)
			_succ[i] = i + 1;
		_free = 1;
	}

	~bindexlist(void)
	{

	}

	iterator begin(void)
	{
		return iterator(this, _succ[0]);
	}

	iterator end(void)
	{
		return iterator(this, 0);
	}

	T & front(void)
	{
		return _data[_succ[0]];
	}

	const T & front(void) const
	{
		return _data[_succ[0]];
	}

	T & back(void)
	{
		return _data[_pred[0]];
	}

	const T & back(void) const
	{
		return _data[_pred[0]];
	}

	iterator erase(iterator it)
	{
		char s = _succ[it._i];
		_succ[_pred[it._i]] = _pred[it._i];
		_pred[_succ[it._i]] = s;
		_succ[it._i] = _free;
		_free = it._i;
		return iterator(this, s);
	}

	iterator erase(iterator first, iterator last)
	{
		char s = _succ[last._i];
		_succ[_pred[last._i]] = _pred[first._i];
		_pred[_succ[first._i]] = s;
		_succ[last._i] = _free;
		_free = first._i;
		return iterator(this, s);
	}

	void pop_front(void)
	{
		char i = _succ[0];
		char s = _succ[i];
		_pred[s] = 0;
		_succ[0] = s;
		_succ[i] = _free;
		_free = i;
	}

	void pop_back(void)
	{
		char i = _pred[0];
		char p = _pred[i];
		_succ[p] = 0;
		_pred[0] = p;
		_pred[i] = _free;
		_free = i;
	}

	void push_back(const T & t)
	{
		char i = _free;
		_data[i] = t;

		_free = _succ[_free];

		_succ[i] = 0;
		_pred[i] = _pred[0];
		_succ[_pred[0]] = i;
		_pred[0] = i;
	}

	void push_front(const T & t)
	{
		char i = _free;
		_data[i] = t;
		
		_free = _succ[_free];
		_succ[i] = 0;
		_succ[i] = _succ[0];
		_pred[_succ[0]] = i;
		_succ[0] = i;
	}

};


#endif
