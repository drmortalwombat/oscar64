#ifndef OPP_FUNCTIONAL_H
#define OPP_FUNCTIONAL_H

namespace opp
{
template <class F>
class function;

template <class R, class ... P>
class function<R(P...)>
{
private:
	struct callif
	{
		virtual R call(P...) = 0;
		virtual ~callif() {}
		virtual callif * clone(void) const = 0;
	};

	template<class CA>
	struct callable : callif
	{
		CA 	ca;

		callable(CA ca_) : ca(ca_) {}

		R call(P... p) {return ca(p...);}

		callif * clone(void) const
		{
			return new callable(ca);
		}
	};

   callif	* c;
public:
	template <class F> 
	function(F f)
	{
		c = new callable<F>(f);
	}

	function(const function & f)
	{
		c = f.c->clone();
	}

	function(function && f)
	{
		c = f.c;
		f.c = nullptr;
	}

	function(void)
	{
		c = nullptr;
	}

	function & operator=(const function & f)
	{
		if (c != f.c)
		{
			delete c;
			c = f.c;
		}
		return *this;
	}

	function & operator=(function && f)
	{
		if (c != f.c)
		{
			c = f.c;
			f.c = nullptr;
		}
		return *this;
	}

	~function(void)
	{
		delete c;
	}

	R operator()(P ... p) const
	{ 
		return c->call(p...); 
	}
};


}

#endif
