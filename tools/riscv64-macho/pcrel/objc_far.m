// the objc-test shape: selector and class refs held in callee-saved registers across 4KB
__attribute__((objc_root_class))
@interface Obj
+ (id)make;
- (int)step:(int)i;
- (int)total;
@end
int far_sends(int n)
{
	int sum = 0;
	for (int i = 0; i < n; i++) {
		id o = [Obj make];
		sum += [o step:i];
		__asm__ volatile(".fill 1100, 4, 0x00000013");
		sum += [o total] + [o step:sum];
	}
	return sum;
}
