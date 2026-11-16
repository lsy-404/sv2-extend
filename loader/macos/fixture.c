#include <dispatch/dispatch.h>
#include <stdlib.h>
static void finish(void *unused) { (void)unused; exit(0); }
int main(void) {
    dispatch_after_f(dispatch_time(DISPATCH_TIME_NOW, 1000000000), dispatch_get_main_queue(), NULL, finish);
    dispatch_main();
}
