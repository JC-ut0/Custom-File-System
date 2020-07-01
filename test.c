#include <stdio.h>
#include <string.h>
// #include "a1fs.h"
#include <sys/types.h>

int main(){
    
    char bit_map[2] = {-1,-1};
    // unsigned char bit_map[2] = {1,1};
    int index = 1;
	char *byte = bit_map + (index / 8);
	*byte = *byte & ~(1 << (index % 8));

    // for (size_t i = 0; i < 8; i++)
    // {
    //     // printf("%d", 1 && *byte & 1 <<i);
        
    // }
    printf("%d",1 && (*byte & (1 << (index % 8))));
    
    
    return 0;
}