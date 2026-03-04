#include <std.h>

// Sieve of Eratosthenes

enum { N = 10000 };

unsigned char sieve[N];
memset(sieve, 1, sizeof sieve);
sieve[0] = 0;
sieve[1] = 0;

for(int i = 2; i * i < N; i++){
    if(!sieve[i]) continue;
    for(int j = i * i; j < N; j += i)
        sieve[j] = 0;
}

int count = 0;
for(int i = 2; i < N; i++){
    if(sieve[i]){
        printf("%5d ", i);
        count++;
        if(count % 15 == 0) putchar('\n');
    }
}
if(count % 15 != 0) putchar('\n');
printf("\n%d primes below %d\n", count, N);
