#include <stdio.h>
#include <stdlib.h>

void
f(void)
{
    int a[4];
    int *b = (int *)malloc(16);
    int *c;
    int i;
//第一行 1: a = %p, b = %p, c = %p
    printf("1: a = %p, b = %p, c = %p\n", a, b, c);

    c = a;
    for (i = 0; i < 4; i++)
	a[i] = 100 + i;
    c[0] = 200;
//第二行 2: a[0] = %d, a[1] = %d, a[2] = %d, a[3] = %d
    printf("2: a[0] = %d, a[1] = %d, a[2] = %d, a[3] = %d\n",
	   a[0], a[1], a[2], a[3]);

    c[1] = 300;
    *(c + 2) = 301;
    3[c] = 302;
//第三行 3: a[0] = %d, a[1] = %d, a[2] = %d, a[3] = %d
    printf("3: a[0] = %d, a[1] = %d, a[2] = %d, a[3] = %d\n",
	   a[0], a[1], a[2], a[3]);

    c = c + 1;
    *c = 400;
//第四行 4: a[0] = %d, a[1] = %d, a[2] = %d, a[3] = %d
    printf("4: a[0] = %d, a[1] = %d, a[2] = %d, a[3] = %d\n",
	   a[0], a[1], a[2], a[3]);

    c = (int *) ((char *) c + 1);
    *c = 500;
//第五行 5: a[0] = %d, a[1] = %d, a[2] = %d, a[3] = %d
    printf("5: a[0] = %d, a[1] = %d, a[2] = %d, a[3] = %d\n",
	   a[0], a[1], a[2], a[3]);

    b = (int *) a + 1;
    c = (int *) ((char *) a + 1);
//第六行 6: a = %p, b = %p, c = %p
    printf("6: a = %p, b = %p, c = %p\n", a, b, c);
}

int
main(int ac, char **av)
{
    f();
    return 0;
}