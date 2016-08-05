#ifndef __defer_h__
#define __defer_h__

#include <utility>
#include <functional>


class defer {
public:
	typedef std::function<void()> FX;
	defer() = default;

	defer(FX &&fx) : _fx(std::forward<FX>(fx)) {}
	defer(const defer &) = delete;
	defer(defer &&) = default;
	defer & operator=(const defer &) = delete;
	defer & operator=(defer &&) = default;

	void cancel() { _fx = nullptr;  }
	~defer() { if (_fx) _fx(); }
private:
	FX _fx;
};


#endif
