
int func() {
	return 0;
}

__typeof(&func) funcPtr = &func;
int _start() {
	return funcPtr();
}