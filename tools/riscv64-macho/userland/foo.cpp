// a c++ dylib: exported functions and data, a thread local, a polymorphic class with rtti and exceptions
extern "C" int printf(const char *, ...);
struct Error { int code; };
class Shape {
public:
	virtual ~Shape() {}
	virtual int area() const = 0;
};
class Square : public Shape {
	int s;
public:
	explicit Square(int v) : s(v) {}
	int area() const override { if (s < 0) throw Error{s}; return s * s; }
};
extern "C" {
int foo_counter = 7;
const char *foo_name = "foo";
_Thread_local int foo_tls = 3;
int foo_area(int side) {
	Square sq(side);
	Shape &sh = sq;
	try {
		return sh.area() + (dynamic_cast<Square *>(&sh) ? 1 : 0);
	} catch (const Error &e) {
		return e.code;
	}
}
int foo_bump(void) { return ++foo_tls + foo_counter; }
}
