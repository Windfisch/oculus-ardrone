#include <math.h>

class Ringbuffer
{
	public:

	Ringbuffer(int size)
	{
		this->size = size;
		idx = 0;
		buf = new double[size];
		for (int i=0; i<size; i++)
			buf[i] = 0;
		avg = 0.0;
		avg_valid = false;
	}

	~Ringbuffer()
	{
		delete [] buf;
	}

	double get()
	{
		if (!avg_valid)
		{
			avg=0.0;
			for (int i=0; i<size; i++)
				avg += buf[i];
			avg/=size;
			avg_valid=true;
		}

		return avg;
	}

	void put(double val)
	{
		buf[idx] = val;
		idx = (idx+1) % size;
		avg_valid = false;
	}

	void set(double val)
	{
		for (int i=0; i<size; i++)
			buf[i]=val;
		avg = val;
		avg_valid=true;
	}

	void add(double val)
	{
		for (int i=0; i<size; i++)
			buf[i]+=val;
		avg += val;
	}

	private:
		double* buf;
		int idx;
		int size;
		double avg;
		bool avg_valid;
};

class ModuloRingbuffer
{
	private:
		Ringbuffer* rb;
		double low,upp,span;

	public:
	
	ModuloRingbuffer(int size, double low, double upp)
	{
		rb = new Ringbuffer(size);
		this->low=low;
		this->upp=upp;
		this->span=upp-low;
		rb->set(_fixup_range(0.0));
	}

	~ModuloRingbuffer()
	{
		delete rb;
	}

	double _fixup_range(double val)
	{
		while (val < low) val+=span;
		while (val>= upp) val-=span;
		return val;
	}

	double get()
	{
		return rb->get();
	}

	void put(double val)
	{
		val=_fixup_range(val);

		// direct way
		double dist1 = val - get();
		// over borders
		double dist2 = dist1 + span;
		double dist3 = dist1 - span;

		if (fabs(dist1) <= fabs(dist2) && fabs(dist1) <= fabs(dist3))
			rb->put(val);
		else if (fabs(dist2) <= fabs(dist3))
			rb->put(val+span);
		else
			rb->put(val-span);

		while (get() < low) rb->add(span);
		while (get()>= upp) rb->add(-span);
	}

	void set(double val)
	{
		rb->set(_fixup_range(val));
	}

	void add(double val)
	{
		rb->add(val);
		
		while (get() < low) rb->add(span);
		while (get()>= upp) rb->add(-span);
	}
};
