// the client: calls into the dylib, reads its data and tls through imports
extern int foo_counter;
extern const char *foo_name;
int foo_area(int);
int foo_bump(void);
int puts(const char *);
int main(void) { puts(foo_name); return foo_area(3) + foo_bump() + foo_counter; }
