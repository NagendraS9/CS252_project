
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main()
{
	char* str = "abcdefghijklmnop";
	char* str2 = "hello";
	char* x = malloc(20*sizeof(*x));
	char* y = malloc(10*sizeof(*y));
	strcpy(x, str);
	strcpy(y, str2);
	
	fflush(stdout);
	
	// fflush(stdout);
	// char str[100] = "Learningisfun";
 //    char *first, *second;
 //    first = str;
 //    second = str;
 //    printf("Original string :%s\n ", str);
      
 //    // when overlap happens then it just ignore it
 //    memcpy(first + 8, first, 10);
 //    printf("memcpy overlap : %s\n ", str);
	char c1 = 'A';
	char c2 = 'B';

	unsigned short xx = (c1<<8)+c2; 
	memcpy(x+3, &xx, sizeof(short));
	for(int i=0; i<20; i++)
		printf("%d:%c,%d\n", i, x[i], x[i]);

  
}