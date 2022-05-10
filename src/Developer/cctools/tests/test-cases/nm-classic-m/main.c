extern void foo(void);
extern void fooSuffix(void);
extern void fooPath(void);
extern void fooPathSuffix(void);
extern void fooVers(void);
extern void fooPathVers(void);
extern void x(void);
extern void xSuffix(void);
extern void xPathSuffix(void);
extern void atsVersSuffix(void);
extern void atsPathVersSuffix(void);
extern void qt(void);
extern void qtPath(void);
extern void foo_bar(void);
extern void foo_barSuffix(void);

int
main(){
  foo();
  fooSuffix();
  fooPath();
  fooPathSuffix();
  fooVers();
  fooPathVers();
  x();
  xSuffix();
  xPathSuffix();
  atsVersSuffix();
  atsPathVersSuffix();
  qt();
  qtPath();
  foo_bar();
  foo_barSuffix();
  return 0;
}
