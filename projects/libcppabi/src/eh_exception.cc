
#include <exception>
#include <cxxabi.h>


namespace std {



exception::~exception() throw() 
{ 
}

bad_exception::~bad_exception() throw() 
{ 
}

const char* exception::what() const throw()
{
  return "std::exception";
}

const char* bad_exception::what() const throw()
{
  return "std::bad_exception";
}


class exception_ptr current_exception();
void rethrow_exception(class exception_ptr) __attribute__((noreturn));

class exception_ptr
{
    void* __ptr_;
public:
    exception_ptr()  : __ptr_() {}
    exception_ptr(const exception_ptr&);
    exception_ptr& operator=(const exception_ptr&);
    ~exception_ptr();

    friend exception_ptr current_exception();
    friend void rethrow_exception(exception_ptr) __attribute__((noreturn));
};



exception_ptr current_exception()
{
	// be nicer if there was a constructor that took a ptr, then 
	// this whole function would be just:
	//    return exception_ptr(__cxa_current_primary_exception());
    exception_ptr ptr;
	ptr.__ptr_ = __cxxabiapple::__cxa_current_primary_exception();
	return ptr;
}

void rethrow_exception(exception_ptr p)
{
	__cxxabiapple::__cxa_rethrow_primary_exception(p.__ptr_); 
	// if p.__ptr_ is NULL, above returns so we terminate
    terminate(); 
}

exception_ptr::~exception_ptr()
{
    __cxxabiapple::__cxa_decrement_exception_refcount(__ptr_);
}

exception_ptr::exception_ptr(const exception_ptr& other)
    : __ptr_(other.__ptr_)
{
    __cxxabiapple::__cxa_increment_exception_refcount(__ptr_);
}

exception_ptr& exception_ptr::operator=(const exception_ptr& other)
{
    if (__ptr_ != other.__ptr_)
    {
        __cxxabiapple::__cxa_increment_exception_refcount(other.__ptr_);
        __cxxabiapple::__cxa_decrement_exception_refcount(__ptr_);
		__ptr_ = other.__ptr_;
	}
    return *this;
}

} // std
