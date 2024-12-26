#ifndef OPP_BOUNDINT_H
#define OPP_BOUNDINT_H

namespace opp {
	
	template<int tmin, int tmax>
	constexpr auto boundinttype(void)
	{
		if constexpr (tmin >= 0 && tmax <= 255)
			return (char)0;
		else if constexpr (tmin >= -128 && tmax <= 127)
			return (signed char)0;
		else
			return (int)0;
	}

	template<int tmin, int tmax> 
	class boundint
	{
	protected:
		decltype(boundinttype<tmin, tmax>()) 	v;
	public:
		boundint(int i) 
			: v(i) 
		{

		}

		void operator=(int k)
		{
			__assume(k >= tmin && k <= tmax);
			v = k;
		}

		operator int() const
		{
			int k = v;
			__assume(k >= tmin && k <= tmax);
			return k;
		}
	};

}

#endif
