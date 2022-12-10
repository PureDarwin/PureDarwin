
// Add a large buffer so that we know we have a bunch of stuff in __DATA
// and can more easily see that all the segments have moved around correctly, not just
// got lucky that all are the same size
__attribute__((used))
char buffer[1024 * 16];

int f = 0;
int *gs[] = { &f, &f, 0, (int*)&buffer[0], (int*)&buffer[1] };

int _start() {
	return *gs[0] + *gs[1];;
}
